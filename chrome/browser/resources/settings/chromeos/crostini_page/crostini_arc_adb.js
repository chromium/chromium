// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-arc-adb' is the ARC adb sideloading subpage for Crostini.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import './crostini_arc_adb_confirmation_dialog.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../../settings_shared.css.js';

import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/cr_elements/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsCrostiniArcAdbElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteObserverBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsCrostiniArcAdbElement extends SettingsCrostiniArcAdbElementBase {
  static get is() {
    return 'settings-crostini-arc-adb';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {boolean} */
      arcAdbEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the device requires a powerwash first (to define nvram for boot
       * lockbox). This happens to devices initialized through OOBE flow before
       * M74.
       * @private {boolean}
       */
      arcAdbNeedPowerwash_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      isOwnerProfile_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isOwnerProfile');
        },
      },

      /** @private {boolean} */
      isEnterpriseManaged_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isEnterpriseManaged');
        },
      },

      /** @private {boolean} */
      canChangeAdbSideloading_: {
        type: Boolean,
        value: false,
      },

      /** @private {boolean} */
      showConfirmationDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kCrostiniAdbDebugging]),
      },
    };
  }

  constructor() {
    super();

    /** @private {!CrostiniBrowserProxy} */
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'crostini-arc-adb-sideload-status-changed',
        (enabled, need_powerwash) => {
          this.arcAdbEnabled_ = enabled;
          this.arcAdbNeedPowerwash_ = need_powerwash;
        });

    this.addWebUIListener(
        'crostini-can-change-arc-adb-sideload-changed',
        (can_change_arc_adb_sideloading) => {
          this.canChangeAdbSideloading_ = can_change_arc_adb_sideloading;
        });

    this.browserProxy_.requestArcAdbSideloadStatus();

    this.browserProxy_.getCanChangeArcAdbSideloading();
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.CROSTINI_ANDROID_ADB) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Returns whether the toggle is changeable by the user. See
   * CrostiniFeatures::CanChangeAdbSideloading(). Note that the actual
   * guard should be in the browser, otherwise a user may bypass this check by
   * inspecting Settings with developer tools.
   * @return {boolean} Whether the control should be disabled.
   * @private
   */
  shouldDisable_() {
    return !this.canChangeAdbSideloading_ || this.arcAdbNeedPowerwash_;
  }

  /**
   * @return {CrPolicyIndicatorType} Which policy indicator to show (if any).
   * @private
   */
  getPolicyIndicatorType_() {
    if (this.isEnterpriseManaged_) {
      if (this.canChangeAdbSideloading_) {
        return CrPolicyIndicatorType.NONE;
      } else {
        return CrPolicyIndicatorType.DEVICE_POLICY;
      }
    } else if (!this.isOwnerProfile_) {
      return CrPolicyIndicatorType.OWNER;
    } else {
      return CrPolicyIndicatorType.NONE;
    }
  }

  /**
   * @return {string} Which action to perform when the toggle is changed.
   * @private
   */
  getToggleAction_() {
    return this.arcAdbEnabled_ ? 'disable' : 'enable';
  }

  /** @private */
  onArcAdbToggleChanged_() {
    this.showConfirmationDialog_ = true;
  }

  /** @private */
  onConfirmationDialogClose_() {
    this.showConfirmationDialog_ = false;
    this.$.arcAdbEnabledButton.checked = this.arcAdbEnabled_;
  }
}

customElements.define(
    SettingsCrostiniArcAdbElement.is, SettingsCrostiniArcAdbElement);
