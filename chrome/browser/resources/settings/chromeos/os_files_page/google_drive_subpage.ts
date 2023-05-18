// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {GoogleDriveBrowserProxy, Stage, Status} from './google_drive_browser_proxy.js';
import {getTemplate} from './google_drive_subpage.html.js';

const SettingsGoogleDriveSubpageElementBase =
    I18nMixin(PrefsMixin(DeepLinkingMixin(RouteObserverMixin(PolymerElement))));

/**
 * The preference containing the value whether Google drive is disabled or not.
 */
const GOOGLE_DRIVE_DISABLED_PREF = 'gdata.disabled';

/**
 * The preference containing the value whether bulk pinning is enabled or not.
 */
const GOOGLE_DRIVE_BULK_PINNING_PREF = 'drivefs.bulk_pinning_enabled';

/**
 * A list of possible confirmation dialogs that may be shown.
 */
export enum ConfirmationDialogType {
  DISCONNECT = 'disconnect',
  BULK_PINNING_DISABLE = 'bulk-pinning-disable',
  BULK_PINNING_NOT_ENOUGH_SPACE = 'bulk-pinning-not-enough-space',
  BULK_PINNING_UNEXPECTED_ERROR = 'bulk-pinning-unexpected-error',
  BULK_PINNING_CLEAR_FILES = 'bulk-pinning-clear-files',
  NONE = 'none',
}

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
       * Ensures the data binding is updated on the UI when `totalPinnedSize_`
       * is updated.
       */
      totalPinnedSize_: String,
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
   * A connection with the browser process to send/receive messages.
   */
  private proxy_: GoogleDriveBrowserProxy;

  /**
   * Keeps track of the latest response about bulk pinning from the page
   * handler.
   */
  private bulkPinningStatus_: Status|null = null;

  /**
   * If the underlying service is unavailable, this will get set to true.
   */
  private bulkPinningServiceUnavailable_: boolean = false;

  /**
   * Maps the dialogType_ property.
   */
  private dialogType_: ConfirmationDialogType = ConfirmationDialogType.NONE;

  /**
   * Keeps track of the last requested total pinned size.
   */
  private totalPinnedSize_: string|null = null;

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
    return this.bulkPinningStatus_?.requiredSpace || '-1';
  }

  /**
   * Returns the remaining space that is currently stored or -1 of no value.
   * Used for testing.
   */
  get remainingSpace() {
    return this.bulkPinningStatus_?.remainingSpace || '-1';
  }

  /**
   * Returns the total pinned size stored.
   * Used for testing.
   */
  get totalPinnedSize() {
    return this.totalPinnedSize_;
  }

  /**
   * Returns the current confirmation dialog showing.
   */
  get dialogType() {
    return this.dialogType_;
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
    if (status.stage !== this.bulkPinningStatus_?.stage ||
        status.remainingSpace !== this.bulkPinningStatus_?.remainingSpace ||
        status.requiredSpace !== this.bulkPinningStatus_?.requiredSpace) {
      this.bulkPinningStatus_ = status;
    }
  }

  /**
   * Retrieves the total pinned size of items in Drive and stores the total.
   */
  private async updateTotalPinnedSize_() {
    this.totalPinnedSize_ =
        this.i18n('googleDriveOfflineClearCalculatingSubtitle');
    const {size} = await this.pageHandler.getTotalPinnedSize();
    if (size) {
      this.totalPinnedSize_ = size;
      return;
    }
    this.totalPinnedSize_ = this.i18n('googleDriveOfflineClearErrorSubtitle');
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

    this.onNavigated();
  }

  /**
   * Invokes methods when the route is navigated to.
   */
  onNavigated() {
    this.attemptDeepLink();
    this.pageHandler.calculateRequiredSpace();
    this.updateTotalPinnedSize_();
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
    this.dialogType_ = ConfirmationDialogType.DISCONNECT;
  }

  /**
   * Update the `gdata.disabled` pref to `true` iff the "Disconnect" button was
   * pressed, all remaining actions (e.g. Cancel, ESC) should not update the
   * preference.
   */
  private async onDriveConfirmationDialogClose_(e: CustomEvent) {
    const closedDialogType = this.dialogType_;
    this.dialogType_ = ConfirmationDialogType.NONE;
    if (!e.detail.accept) {
      return;
    }

    switch (closedDialogType) {
      case ConfirmationDialogType.DISCONNECT:
        this.setPrefValue(GOOGLE_DRIVE_DISABLED_PREF, true);
        break;
      case ConfirmationDialogType.BULK_PINNING_DISABLE:
        this.setPrefValue(GOOGLE_DRIVE_BULK_PINNING_PREF, false);
        break;
      case ConfirmationDialogType.BULK_PINNING_CLEAR_FILES:
        await this.proxy_.handler.clearPinnedFiles();
        this.updateTotalPinnedSize_();
        break;
      default:
        // All other dialogs currently do not require any action (only a
        // cancellation) and so should not be reached.
        assertNotReached('Unknown acceptance criteria from dialog');
    }
  }

  /**
   * Returns the sublabel for the bulk pinning preference toggle. If the
   * required / free space has been calculated, includes the values in the
   * sublabel.
   */
  private getBulkPinningSubLabel_(): string {
    if (!this.bulkPinningStatus_ ||
        this.bulkPinningStatus_?.stage !== Stage.kSuccess ||
        this.bulkPinningServiceUnavailable_) {
      return this.i18n('googleDriveOfflineSubtitle');
    }

    const {requiredSpace, remainingSpace} = this.bulkPinningStatus_;
    return this.i18n('googleDriveOfflineSubtitle') + ' ' +
        this.i18n(
            'googleDriveOfflineSpaceSubtitle', requiredSpace!, remainingSpace!);
  }

  /**
   * For the various dialogs that are defined in the HTML, only one should be
   * shown at all times. If the supplied type matches the requested type, show
   * the dialog.
   */
  private shouldShowConfirmationDialog_(
      type: ConfirmationDialogType, requestedType: string) {
    return type === requestedType;
  }

  /**
   * Spawn a confirmation dialog to the user if they choose to disable the bulk
   * pinning feature, for enabling just update the preference.
   */
  private onToggleBulkPinning_(e: Event) {
    const target = e.target as SettingsToggleButtonElement;
    const newValueAfterToggle =
        !this.getPref(GOOGLE_DRIVE_BULK_PINNING_PREF).value;

    if (this.bulkPinningStatus_?.isError) {
      target.checked = false;
      // If there is not enough free space for the user to reliably turn on bulk
      // pinning, spawn a dialog.
      if (this.bulkPinningStatus_?.stage === Stage.kNotEnoughSpace) {
        this.dialogType_ = ConfirmationDialogType.BULK_PINNING_NOT_ENOUGH_SPACE;
        return;
      }

      // If an error occurs (that is not related to low disk space) surface an
      // unexpected error dialog.
      this.dialogType_ = ConfirmationDialogType.BULK_PINNING_UNEXPECTED_ERROR;
      return;
    }

    target.checked = true;

    // Turning the preference off should first spawn a dialog to have the user
    // confirm that is what they want to do, leave the target as checked as the
    // user must confirm before the preference gets updated.
    if (!newValueAfterToggle) {
      this.dialogType_ = ConfirmationDialogType.BULK_PINNING_DISABLE;
      return;
    }

    this.setPrefValue(GOOGLE_DRIVE_BULK_PINNING_PREF, true);
  }

  /**
   * Returns true if the bulk pinning preference is disabled.
   */
  private shouldEnableClearOfflineButton_() {
    return this.getPref(GOOGLE_DRIVE_BULK_PINNING_PREF).value;
  }

  /**
   * Returns the string used in the confirmation dialog when clearing the users
   * offline storage, this includes the total GB used by offline files.
   */
  private getClearOfflineStorageConfirmationBody_() {
    return this.i18n(
        'googleDriveOfflineClearDialogBody', this.totalPinnedSize_!);
  }

  /**
   * When the "Clear offline storage" button is clicked, should not clear
   * immediately but show the confirmation dialog first.
   */
  private async onClearPinnedFiles_() {
    this.dialogType_ = ConfirmationDialogType.BULK_PINNING_CLEAR_FILES;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-google-drive-subpage': SettingsGoogleDriveSubpageElement;
  }
}

customElements.define(
    SettingsGoogleDriveSubpageElement.is, SettingsGoogleDriveSubpageElement);
