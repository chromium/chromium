// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import './multidevice_feature_toggle.js';
import './multidevice_radio_button.js';
import '../../settings_shared.css.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/cr_elements/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {OsSettingsRoutes} from '../os_settings_routes.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_browser_proxy.js';
import {MultiDeviceFeature, MultiDeviceFeatureState, SmartLockSignInEnabledState} from './multidevice_constants.js';
import {MultiDeviceFeatureBehavior, MultiDeviceFeatureBehaviorInterface} from './multidevice_feature_behavior.js';
import {recordSmartLockToggleMetric, SmartLockToggleLocation} from './multidevice_metrics_logger.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {MultiDeviceFeatureBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsMultideviceSmartlockSubpageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      MultiDeviceFeatureBehavior,
      RouteObserverBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsMultideviceSmartlockSubpageElement extends
    SettingsMultideviceSmartlockSubpageElementBase {
  static get is() {
    return 'settings-multidevice-smartlock-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {?OsSettingsRoutes} */
      routes: {
        type: Object,
        value: routes,
      },

      /**
       * True if Smart Lock is enabled.
       * @private
       */
      smartLockEnabled_: {
        type: Boolean,
        computed: 'computeIsSmartLockEnabled_(pageContentData)',
      },

      /**
       * Whether Smart Lock may be used to sign-in the user (as opposed to only
       * being able to unlock the user's screen).
       * @private {!SmartLockSignInEnabledState}
       */
      smartLockSignInEnabled_: {
        type: Object,
        value: SmartLockSignInEnabledState.DISABLED,
      },

      /**
       * True if the user is allowed to enable Smart Lock sign-in.
       * @private
       */
      smartLockSignInAllowed_: {
        type: Boolean,
        value: true,
      },

      /** @private */
      showPasswordPromptDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Authentication token provided by password-prompt-dialog.
       * @private {!chrome.quickUnlockPrivate.TokenInfo|undefined}
       */
      authToken_: {
        type: Object,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kSmartLockOnOff,
          Setting.kSmartLockUnlockOrSignIn,
        ]),
      },
    };
  }

  constructor() {
    super();

    /** @private {!MultiDeviceBrowserProxy} */
    this.browserProxy_ = MultiDeviceBrowserProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener('feature-toggle-clicked', (event) => {
      this.onFeatureToggleClicked_(
          /**
           * @type {!CustomEvent<!{feature: !MultiDeviceFeature, enabled:
           *  boolean}>}
           */
          (event));
    });

    this.addWebUIListener(
        'smart-lock-signin-enabled-changed',
        this.updateSmartLockSignInEnabled_.bind(this));

    this.addWebUIListener(
        'smart-lock-signin-allowed-changed',
        this.updateSmartLockSignInAllowed_.bind(this));

    this.browserProxy_.getSmartLockSignInEnabled().then(enabled => {
      this.updateSmartLockSignInEnabled_(enabled);
    });

    this.browserProxy_.getSmartLockSignInAllowed().then(allowed => {
      this.updateSmartLockSignInAllowed_(allowed);
    });
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.SMART_LOCK) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Returns true if Smart Lock is an enabled feature.
   * @return {boolean}
   * @private
   */
  computeIsSmartLockEnabled_() {
    return !!this.pageContentData &&
        this.getFeatureState(MultiDeviceFeature.SMART_LOCK) ===
        MultiDeviceFeatureState.ENABLED_BY_USER;
  }

  /**
   * Updates the state of the Smart Lock 'sign-in enabled' toggle.
   * @private
   */
  updateSmartLockSignInEnabled_(enabled) {
    this.smartLockSignInEnabled_ = enabled ?
        SmartLockSignInEnabledState.ENABLED :
        SmartLockSignInEnabledState.DISABLED;
  }

  /**
   * Updates the Smart Lock 'sign-in enabled' toggle such that disallowing
   * sign-in disables the toggle.
   * @private
   */
  updateSmartLockSignInAllowed_(allowed) {
    this.smartLockSignInAllowed_ = allowed;
  }

  /** @private */
  openPasswordPromptDialog_() {
    this.showPasswordPromptDialog_ = true;
  }

  /**
   * Sets the Smart Lock 'sign-in enabled' pref based on the value of the
   * radio group representing the pref.
   * @private
   */
  onSmartLockSignInEnabledChanged_() {
    const radioGroup = this.shadowRoot.querySelector('cr-radio-group');
    const enabled = radioGroup.selected === SmartLockSignInEnabledState.ENABLED;

    if (!enabled) {
      // No authentication check is required to disable.
      this.browserProxy_.setSmartLockSignInEnabled(false /* enabled */);
      recordSettingChange();
      return;
    }

    // Toggle the enabled state back to disabled, as authentication may not
    // succeed. The toggle state updates automatically by the pref listener.
    radioGroup.selected = SmartLockSignInEnabledState.DISABLED;
    this.openPasswordPromptDialog_();
  }

  /**
   * Updates the state of the password dialog controller flag when the UI
   * element closes.
   * @private
   */
  onEnableSignInDialogClose_() {
    this.showPasswordPromptDialog_ = false;

    // If |this.authToken_| is set when the dialog has been closed, this means
    // that the user entered the correct password into the dialog when
    // attempting to enable SignIn with Smart Lock.
    if (this.authToken_) {
      this.browserProxy_.setSmartLockSignInEnabled(
          true /* enabled */, this.authToken_.token);
      recordSettingChange();
    }

    // Always require password entry if re-enabling SignIn with Smart Lock.
    this.authToken_ = undefined;
  }

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   */
  onTokenObtained_(e) {
    this.authToken_ = e.detail;
  }

  /**
   * Intercept (but do not stop propagation of) the feature-toggle-clicked event
   * for the purpose of logging metrics.
   * @private
   */
  onFeatureToggleClicked_(event) {
    const feature = event.detail.feature;
    const enabled = event.detail.enabled;

    if (feature !== MultiDeviceFeature.SMART_LOCK) {
      return;
    }

    const previousRoute = window.history.state &&
        Router.getInstance().getRouteForPath(
            /** @type {string} */ (window.history.state));
    if (!previousRoute) {
      return;
    }

    let toggleLocation = SmartLockToggleLocation.MULTIDEVICE_PAGE;
    if (previousRoute === routes.LOCK_SCREEN) {
      toggleLocation = SmartLockToggleLocation.LOCK_SCREEN_SETTINGS;
    }

    recordSmartLockToggleMetric(toggleLocation, enabled);
  }
}

customElements.define(
    SettingsMultideviceSmartlockSubpageElement.is,
    SettingsMultideviceSmartlockSubpageElement);
