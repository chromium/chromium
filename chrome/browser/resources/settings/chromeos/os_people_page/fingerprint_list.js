// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The duration in ms of a background flash when a user touches the fingerprint
 * sensor on this page.
 * @type {number}
 */
const FLASH_DURATION_MS = 500;

import {afterNextRender, Polymer, html, flush, Templatizer, TemplateInstanceBase} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.m.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '//resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import {FingerprintInfo, FingerprintBrowserProxy, FingerprintResultType, FingerprintBrowserProxyImpl} from './fingerprint_browser_proxy.js';
import './setup_fingerprint_dialog.js';
import {loadTimeData} from '../../i18n_setup.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import '//resources/cr_components/localized_link/localized_link.js';
import {routes} from '../os_route.js';
import {Router, Route} from '../../router.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';
import '../../settings_shared_css.js';
import {recordSettingChange} from '../metrics_recorder.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-fingerprint-list',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
    RouteObserverBehavior,
  ],

  properties: {
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
      }
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
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kAddFingerprintV2,
        chromeos.settings.mojom.Setting.kRemoveFingerprintV2,
      ]),
    },
  },

  /** @private {?FingerprintBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached() {
    this.addWebUIListener('on-screen-locked', this.onScreenLocked_.bind(this));
    this.browserProxy_ = FingerprintBrowserProxyImpl.getInstance();
    this.browserProxy_.startAuthentication();
    this.updateFingerprintsList_();
  },

  /** @override */
  detached() {
    this.browserProxy_.endCurrentAuthentication();
  },

  /**
   * @return {boolean} Whether an event was fired to show the password dialog.
   * @private
   */
  requestPasswordIfApplicable_() {
    const currentRoute = Router.getInstance().getCurrentRoute();
    if (currentRoute === routes.FINGERPRINT && !this.authToken) {
      this.fire('password-requested');
      return true;
    }
    return false;
  },

  /**
   * Overridden from RouteObserverBehavior.
   * @param {!Route} newRoute
   * @param {!Route} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute !== routes.FINGERPRINT) {
      if (this.browserProxy_) {
        this.browserProxy_.endCurrentAuthentication();
      }
      this.showSetupFingerprintDialog_ = false;
      return;
    }

    if (oldRoute === routes.LOCK_SCREEN) {
      // Start fingerprint authentication when going from LOCK_SCREEN to
      // FINGERPRINT page.
      this.browserProxy_.startAuthentication();
    }

    if (this.requestPasswordIfApplicable_()) {
      this.showSetupFingerprintDialog_ = false;
    }

    this.attemptDeepLink();
  },

  /** @private */
  updateFingerprintsList_() {
    this.browserProxy_.getFingerprintsList().then(
        this.onFingerprintsChanged_.bind(this));
  },

  /**
   * @param {!FingerprintInfo} fingerprintInfo
   * @private
   */
  onFingerprintsChanged_(fingerprintInfo) {
    // Update iron-list.
    this.fingerprints_ = fingerprintInfo.fingerprintsList.slice();
    this.$$('.action-button').disabled = fingerprintInfo.isMaxed;
    this.allowAddAnotherFinger_ = !fingerprintInfo.isMaxed;
  },

  /**
   * Deletes a fingerprint from |fingerprints_|.
   * @param {!{model: !{index: !number}}} e
   * @private
   */
  onFingerprintDeleteTapped_(e) {
    this.browserProxy_.removeEnrollment(e.model.index).then(success => {
      if (success) {
        recordSettingChange();
        this.updateFingerprintsList_();
      }
    });
  },

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
  },

  /**
   * Opens the setup fingerprint dialog.
   * @private
   */
  openAddFingerprintDialog_() {
    this.showSetupFingerprintDialog_ = true;
  },

  /** @private */
  onSetupFingerprintDialogClose_() {
    this.showSetupFingerprintDialog_ = false;
    focusWithoutInk(assert(this.$$('#addFingerprint')));
    this.browserProxy_.startAuthentication();
  },

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
  },

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
  },

  /**
   * @param {string} item
   * @return {string}
   * @private
   */
  getButtonAriaLabel_(item) {
    return this.i18n('lockScreenDeleteFingerprintLabel', item);
  },
});
