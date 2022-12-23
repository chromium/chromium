// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import './setup_fingerprint_dialog.js';
import '../../settings_shared.css.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {focusWithoutInk} from 'chrome://resources/ash/common/focus_without_ink_js.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {Route, Router} from '../router.js';

import {FingerprintBrowserProxy, FingerprintBrowserProxyImpl, FingerprintInfo} from './fingerprint_browser_proxy.js';
import {getTemplate} from './fingerprint_list.html.js';

/**
 * The duration in ms of a background flash when a user touches the fingerprint
 * sensor on this page.
 * @type {number}
 */
const FLASH_DURATION_MS = 500;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsFingerprintListElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      WebUIListenerBehavior,
      RouteObserverBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsFingerprintListElement extends
    SettingsFingerprintListElementBase {
  static get is() {
    return 'settings-fingerprint-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Authentication token provided by settings-people-page.
       */
      authToken: {
        type: String,
        value: '',
        observer: 'onAuthTokenChanged_',
      },

      /**
       * The list of fingerprint objects.
       * @private {!Array<string>}
       */
      fingerprints_: {
        type: Array,
        value() {
          return [];
        },
      },

      /** @private */
      showSetupFingerprintDialog_: Boolean,

      /**
       * Whether add another finger is allowed.
       * @type {boolean}
       * @private
       */
      allowAddAnotherFinger_: {
        type: Boolean,
        value: true,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kAddFingerprintV2,
          Setting.kRemoveFingerprintV2,
        ]),
      },
    };
  }

  constructor() {
    super();

    /** @private {!FingerprintBrowserProxy} */
    this.browserProxy_ = FingerprintBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener('on-screen-locked', this.onScreenLocked_.bind(this));
    this.updateFingerprintsList_();
  }

  /**
   * @return {boolean} Whether an event was fired to show the password dialog.
   * @private
   */
  requestPasswordIfApplicable_() {
    const currentRoute = Router.getInstance().getCurrentRoute();
    if (currentRoute === routes.FINGERPRINT && !this.authToken) {
      const event = new CustomEvent(
          'password-requested', {bubbles: true, composed: true});
      this.dispatchEvent(event);
      return true;
    }
    return false;
  }

  /**
   * Overridden from RouteObserverBehavior.
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute !== routes.FINGERPRINT) {
      this.showSetupFingerprintDialog_ = false;
      return;
    }

    if (this.requestPasswordIfApplicable_()) {
      this.showSetupFingerprintDialog_ = false;
    }

    this.attemptDeepLink();
  }

  /** @private */
  updateFingerprintsList_() {
    this.browserProxy_.getFingerprintsList().then(
        this.onFingerprintsChanged_.bind(this));
  }

  /**
   * @param {!FingerprintInfo} fingerprintInfo
   * @private
   */
  onFingerprintsChanged_(fingerprintInfo) {
    // Update iron-list.
    this.fingerprints_ = fingerprintInfo.fingerprintsList.slice();
    this.shadowRoot.querySelector('.action-button').disabled =
        fingerprintInfo.isMaxed;
    this.allowAddAnotherFinger_ = !fingerprintInfo.isMaxed;
  }

  /**
   * Deletes a fingerprint from |fingerprints_|.
   * @param {!{model: !{index: !number}}} e
   * @private
   */
  onFingerprintDeleteTapped_(e) {
    this.browserProxy_.removeEnrollment(e.model.index, this.authToken)
        .then(success => {
          if (success) {
            recordSettingChange();
            this.updateFingerprintsList_();
          }
        });
  }

  /**
   * @param {!{model: !{index: !number, item: !string}}} e
   * @private
   */
  onFingerprintLabelChanged_(e) {
    this.browserProxy_.changeEnrollmentLabel(e.model.index, e.model.item)
        .then(success => {
          if (success) {
            this.updateFingerprintsList_();
          }
        });
  }

  /**
   * Opens the setup fingerprint dialog.
   * @private
   */
  openAddFingerprintDialog_() {
    this.showSetupFingerprintDialog_ = true;
  }

  /** @private */
  onSetupFingerprintDialogClose_() {
    this.showSetupFingerprintDialog_ = false;
    focusWithoutInk(assert(this.shadowRoot.querySelector('#addFingerprint')));
  }

  /**
   * Close the setup fingerprint dialog when the screen is unlocked.
   * @param {boolean} screenIsLocked
   * @private
   */
  onScreenLocked_(screenIsLocked) {
    if (!screenIsLocked &&
        Router.getInstance().getCurrentRoute() === routes.FINGERPRINT) {
      this.onSetupFingerprintDialogClose_();
    }
  }

  /** @private */
  onAuthTokenChanged_() {
    if (this.requestPasswordIfApplicable_()) {
      this.showSetupFingerprintDialog_ = false;
      return;
    }

    if (Router.getInstance().getCurrentRoute() === routes.FINGERPRINT) {
      // Show deep links again if the user authentication dialog just closed.
      this.attemptDeepLink();
    }
  }

  /**
   * @param {string} item
   * @return {string}
   * @private
   */
  getButtonAriaLabel_(item) {
    return this.i18n('lockScreenDeleteFingerprintLabel', item);
  }
}

customElements.define(
    SettingsFingerprintListElement.is, SettingsFingerprintListElement);
