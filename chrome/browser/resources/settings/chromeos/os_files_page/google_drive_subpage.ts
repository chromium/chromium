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

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {GoogleDriveBrowserProxy, Status} from './google_drive_browser_proxy.js';
import {getTemplate} from './google_drive_subpage.html.js';

const SettingsGoogleDriveSubpageElementBase =
    I18nMixin(PrefsMixin(DeepLinkingMixin(RouteObserverMixin(PolymerElement))));

/**
 * The preference containing the value whether Google drive is disabled or not.
 */
const GOOGLE_DRIVE_DISABLED_PREF = 'gdata.disabled';

export class SettingsGoogleDriveSubpageElement extends
    SettingsGoogleDriveSubpageElementBase {
  constructor() {
    super();
    this.proxy_ = GoogleDriveBrowserProxy.getInstance();
  }

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
   * A connection with the browser process to send/receive messages.
   */
  private proxy_: GoogleDriveBrowserProxy;

  /**
   * Keeps track of the latest response about bulk pinning from the page
   * handler.
   */
  private bulkPinningStatus_?: Status;

  /**
   * If the underlying service is unavailable, this will get set to true.
   */
  private bulkPinningServiceUnavailable_: boolean = false;

  /**
   * Returns the browser proxy page handler (to invoke functions).
   */
  get pageHandler() {
    return this.proxy_.handler;
  }

  /**
   * Returns the browser proxy callback router (to receive async messages).
   */
  get callbackRouter() {
    return this.proxy_.observer;
  }

  /**
   * Returns the required space that is currently stored or -1 of no value. Used
   * for testing.
   */
  get requiredSpace() {
    return this.bulkPinningStatus_?.requiredSpace || -1;
  }

  /**
   * Returns the remaining space that is currently stored or -1 of no value.
   * Used for testing.
   */
  get remainingSpace() {
    return this.bulkPinningStatus_?.remainingSpace || -1;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.callbackRouter.onServiceUnavailable.addListener(
        this.onServiceUnavailable_.bind(this));
    this.callbackRouter.onProgress.addListener(this.onProgress_.bind(this));
  }


  /**
   * Invoked when the underlying service is not longer available.
   */
  private onServiceUnavailable_() {
    this.bulkPinningServiceUnavailable_ = true;
  }

  /**
   * Invoked when progress has occurred with the underlying pinning operation.
   * This could also end up in an error state (e.g. no free space).
   */
  private onProgress_(status: Status) {
    this.bulkPinningStatus_ = status;
  }

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
    this.pageHandler.calculateRequiredSpace();
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
  private onConnectDisconnectClick_(): void {
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
