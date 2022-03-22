// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-arc-adb' is the ARC adb sideloading subpage for Crostini.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import './crostini_arc_adb_confirmation_dialog.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '../../settings_shared_css.js';

import {CrPolicyIndicatorType} from '//resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniDiskInfo, CrostiniPortActiveSetting, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM, MAX_VALID_PORT_NUMBER, MIN_VALID_PORT_NUMBER, PortState} from './crostini_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-crostini-arc-adb',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
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
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () =>
          new Set([chromeos.settings.mojom.Setting.kCrostiniAdbDebugging]),
    },
  },

  attached() {
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

    CrostiniBrowserProxyImpl.getInstance().requestArcAdbSideloadStatus();

    CrostiniBrowserProxyImpl.getInstance().getCanChangeArcAdbSideloading();
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.CROSTINI_ANDROID_ADB) {
      return;
    }

    this.attemptDeepLink();
  },

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
  },

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
  },

  /**
   * @return {string} Which action to perform when the toggle is changed.
   * @private
   */
  getToggleAction_() {
    return this.arcAdbEnabled_ ? 'disable' : 'enable';
  },

  /** @private */
  onArcAdbToggleChanged_() {
    this.showConfirmationDialog_ = true;
  },

  /** @private */
  onConfirmationDialogClose_() {
    this.showConfirmationDialog_ = false;
    this.$.arcAdbEnabledButton.checked = this.arcAdbEnabled_;
  },
});
