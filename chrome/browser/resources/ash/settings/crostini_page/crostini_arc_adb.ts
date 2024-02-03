// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-arc-adb' is the ARC adb sideloading subpage for Crostini.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import './crostini_arc_adb_confirmation_dialog.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '../settings_shared.css.js';

import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {CrPolicyIndicatorType} from 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {PrefsState} from '../common/types.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {getTemplate} from './crostini_arc_adb.html.js';
import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';

export interface SettingsCrostiniArcAdbElement {
  $: {
    arcAdbEnabledButton: CrToggleElement,
  };
}

const SettingsCrostiniArcAdbElementBase = DeepLinkingMixin(
    RouteObserverMixin(WebUiListenerMixin(I18nMixin(PolymerElement))));

export class SettingsCrostiniArcAdbElement extends
    SettingsCrostiniArcAdbElementBase {
  static get is() {
    return 'settings-crostini-arc-adb';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      arcAdbEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the device requires a powerwash first (to define nvram for boot
       * lockbox). This happens to devices initialized through OOBE flow before
       * M74.
       */
      arcAdbNeedPowerwash_: {
        type: Boolean,
        value: false,
      },

      isOwnerProfile_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isOwnerProfile');
        },
      },

      isEnterpriseManaged_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isEnterpriseManaged');
        },
      },

      canChangeAdbSideloading_: {
        type: Boolean,
        value: false,
      },

      showConfirmationDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kCrostiniAdbDebugging]),
      },
    };
  }

  prefs: PrefsState;
  private arcAdbEnabled_: boolean;
  private arcAdbNeedPowerwash_: boolean;
  private browserProxy_: CrostiniBrowserProxy;
  private canChangeAdbSideloading_: boolean;
  private isEnterpriseManaged_: boolean;
  private isOwnerProfile_: boolean;
  private showConfirmationDialog_: boolean;

  constructor() {
    super();

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'crostini-arc-adb-sideload-status-changed',
        (enabled: boolean, needPowerwash: boolean) => {
          this.arcAdbEnabled_ = enabled;
          this.arcAdbNeedPowerwash_ = needPowerwash;
        });

    this.addWebUiListener(
        'crostini-can-change-arc-adb-sideload-changed',
        (canChangeArcAdbSideloading: boolean) => {
          this.canChangeAdbSideloading_ = canChangeArcAdbSideloading;
        });

    this.browserProxy_.requestArcAdbSideloadStatus();

    this.browserProxy_.getCanChangeArcAdbSideloading();
  }

  override currentRouteChanged(route: Route): void {
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
   * @return Whether the control should be disabled.
   */
  private shouldDisable_(): boolean {
    return !this.canChangeAdbSideloading_ || this.arcAdbNeedPowerwash_;
  }

  /**
   * @return Which policy indicator to show (if any).
   */
  private getPolicyIndicatorType_(): CrPolicyIndicatorType {
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
   * @return Which action to perform when the toggle is changed.
   */
  private getToggleAction_(): string {
    return this.arcAdbEnabled_ ? 'disable' : 'enable';
  }

  private onArcAdbToggleChanged_(): void {
    this.showConfirmationDialog_ = true;
  }

  private onConfirmationDialogClose_(): void {
    this.showConfirmationDialog_ = false;
    this.$.arcAdbEnabledButton.checked = this.arcAdbEnabled_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-arc-adb': SettingsCrostiniArcAdbElement;
  }
}

customElements.define(
    SettingsCrostiniArcAdbElement.is, SettingsCrostiniArcAdbElement);
