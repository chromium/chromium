// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {getTemplate} from './google_drive_subpage.html.js';

const SettingsGoogleDriveSubpageElementBase =
    I18nMixin(PrefsMixin(DeepLinkingMixin(RouteObserverMixin(PolymerElement))));

/**
 * The preference containing the value whether Google drive is disabled or not.
 */
const GOOGLE_DRIVE_DISABLED_PREF = 'gdata.disabled';

export class SettingsGoogleDriveSubpageElement extends
    SettingsGoogleDriveSubpageElementBase {
  static get is() {
    return 'settings-google-drive-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kGoogleDriveConnection]),
      },

      /**
       * Whether to show the confirmation dialog when disconnecting Drive.
       */
      showDisconnectDriveConfirmationDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /**
   * Observe the state of `prefs.gdata.disabled` if it gets changed from another
   * location (e.g. enterprise policy).
   */
  static get observers() {
    return [
      `updateDriveDisabled_(prefs.${GOOGLE_DRIVE_DISABLED_PREF}.*)`,
    ];
  }

  /**
   * Reflects the state of `prefs.gdata.disabled` pref.
   */
  private driveDisabled_: boolean;

  /**
   * Tracks the state of the disconnect confirmation dialog.
   */
  private showDisconnectDriveConfirmationDialog_: boolean;

  /**
   * Invoked when the `prefs.gdata.disabled` preference changes value.
   */
  private updateDriveDisabled_() {
    const disabled = this.getPref(GOOGLE_DRIVE_DISABLED_PREF).value;
    this.driveDisabled_ = disabled;
  }

  override currentRouteChanged(route: Route, _oldRoute?: Route) {
    // Does not apply to this page.
    if (route !== routes.GOOGLE_DRIVE) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Returns the value for the button to Connect/Disconnect Google drive
   * depending on the current state.
   */
  private getConnectDisconnectButtonLabel_(): string {
    return this.driveDisabled_ ? this.i18n('googleDriveConnectLabel') :
                                 this.i18n('googleDriveDisconnectLabel');
  }

  /**
   * If Drive is disconnected, immediately update the preference. If Drive is
   * connected, show the confirmation dialog instead of immediately updating the
   * preference when the button is pressed.
   */
  private onConnectDisconnectTap_(): void {
    if (this.driveDisabled_) {
      this.setPrefValue(GOOGLE_DRIVE_DISABLED_PREF, false);
      return;
    }
    this.showDisconnectDriveConfirmationDialog_ = true;
  }

  /**
   * Update the `gdata.disabled` pref to `true` iff the "Disconnect" button was
   * pressed, all remaining actions (e.g. Cancel, ESC) should not update the
   * preference.
   */
  private onDriveDisconnectConfirmationDialogClose_(e: CustomEvent) {
    this.showDisconnectDriveConfirmationDialog_ = false;
    if (!e.detail.accept) {
      return;
    }

    this.setPrefValue(GOOGLE_DRIVE_DISABLED_PREF, true);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-google-drive-subpage': SettingsGoogleDriveSubpageElement;
  }
}

customElements.define(
    SettingsGoogleDriveSubpageElement.is, SettingsGoogleDriveSubpageElement);
