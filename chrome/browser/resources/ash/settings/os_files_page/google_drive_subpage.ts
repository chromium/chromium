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
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, routes} from '../router.js';

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
  BULK_PINNING_LISTING_FILES = 'bulk-pinning-listing-files',
  BULK_PINNING_NOT_ENOUGH_SPACE = 'bulk-pinning-not-enough-space',
  BULK_PINNING_UNEXPECTED_ERROR = 'bulk-pinning-unexpected-error',
  BULK_PINNING_CLEAN_UP_STORAGE = 'bulk-pinning-clean-up-storage',
  BULK_PINNING_OFFLINE = 'bulk-pinning-offline',
  NONE = 'none',
}

/**
 * When the pinned size is not still calculating or unknown.
 */
enum PinnedSizeType {
  UNKNOWN = 'unknown',
  CALCULATING = 'calculating',
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

      /**
       * Ensures the showSpinner variable is bound to the parent element and
       * updates are propagated as the spinner element is in the parent element.
       */
      showSpinner: {
        type: Boolean,
        notify: true,
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
  private totalPinnedSize_: string|PinnedSizeType = PinnedSizeType.CALCULATING;

  /**
   * The number of files that have currently been listed, this count is the one
   * displayed in the UI which gets updated every 5s from the source at
   * bulkPinningStatus_.listedFiles.
   */
  private listedFiles_: bigint = 0n;

  /**
   * The interval to update listedFiles_.
   */
  private updateListedFilesInterval_: number|undefined = undefined;

  /**
   * Whether to show the spinner in the top right of the settings page.
   */
  private showSpinner: boolean = false;

  private updatePinnedSizeInterval_: number;

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
   * Returns the free space that is currently stored or -1 of no value.
   * Used for testing.
   */
  get freeSpace() {
    return this.bulkPinningStatus_?.freeSpace || '-1';
  }

  /**
   * Returns the total pinned size stored.
   * Used for testing.
   */
  get totalPinnedSize() {
    return this.totalPinnedSize_;
  }

  /**
   * Returns the current number of listed files.
   * Used for testing.
   */
  get listedFiles() {
    return this.listedFiles_;
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

  override disconnectedCallback() {
    clearInterval(this.updatePinnedSizeInterval_);
  }

  /**
   * Invoked when the underlying service is not longer available.
   */
  private onServiceUnavailable_() {
    this.bulkPinningServiceUnavailable_ = true;
    clearInterval(this.updateListedFilesInterval_);
    this.updateListedFilesInterval_ = undefined;
  }

  /**
   * Invoked when progress has occurred with the underlying pinning operation.
   * This could also end up in an error state (e.g. no free space).
   */
  private onProgress_(status: Status) {
    this.bulkPinningServiceUnavailable_ = false;

    if (status.stage !== this.bulkPinningStatus_?.stage ||
        status.freeSpace !== this.bulkPinningStatus_?.freeSpace ||
        status.requiredSpace !== this.bulkPinningStatus_?.requiredSpace ||
        status.listedFiles !== this.bulkPinningStatus_?.listedFiles) {
      this.bulkPinningStatus_ = status;

      if (!this.updateListedFilesInterval_ &&
          status.stage === Stage.kListingFiles) {
        this.listedFiles_ = this.bulkPinningStatus_?.listedFiles || 0n;
        this.updateListedFilesInterval_ = setInterval(() => {
          this.listedFiles_ = this.bulkPinningStatus_?.listedFiles || 0n;
        }, 5000);
      }
    }

    if (status.stage !== Stage.kListingFiles) {
      this.stopUpdatingListedFilesAndClearDialog_();
    }

    let requiredSpace: number;
    try {
      requiredSpace = parseInt(status?.requiredSpace);
    } catch (e) {
      console.error('Could not parse required space', e);
      return;
    }

    this.showSpinner = (status?.stage === Stage.kSyncing && requiredSpace > 0);
  }

  /**
   * Whilst listing files an interval is maintained to not update the UI with
   * too many changes. When listing files has finished, ensure the interval is
   * cleared and the dialog is closed if it is kept open.
   */
  private stopUpdatingListedFilesAndClearDialog_() {
    clearInterval(this.updateListedFilesInterval_);
    this.updateListedFilesInterval_ = undefined;
    this.listedFiles_ = 0n;
    if (this.dialogType_ ===
        ConfirmationDialogType.BULK_PINNING_LISTING_FILES) {
      this.dialogType_ = ConfirmationDialogType.NONE;
    }
  }

  /**
   * Retrieves the total pinned size of items in Drive and stores the total.
   */
  private async updateTotalPinnedSize_() {
    if (!this.totalPinnedSize_) {
      // Only set the total pinned size to calculating on the first update.
      this.totalPinnedSize_ = PinnedSizeType.CALCULATING;
    }
    const {size} = await this.pageHandler.getTotalPinnedSize();
    if (size) {
      this.totalPinnedSize_ = size;
      return;
    }
    this.totalPinnedSize_ = PinnedSizeType.UNKNOWN;
  }

  /**
   * Invoked when the `prefs.gdata.disabled` preference changes value.
   */
  private updateDriveDisabled_() {
    const disabled = this.getPref(GOOGLE_DRIVE_DISABLED_PREF).value;
    this.driveDisabled_ = disabled;
    if (disabled) {
      this.showSpinner = false;
    }
  }

  override currentRouteChanged(route: Route, _oldRoute?: Route) {
    // Does not apply to this page.
    if (route !== routes.GOOGLE_DRIVE) {
      clearInterval(this.updatePinnedSizeInterval_);
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
    clearInterval(this.updatePinnedSizeInterval_);
    this.updatePinnedSizeInterval_ =
        setInterval(this.updateTotalPinnedSize_.bind(this), 5000);
  }

  /**
   * Returns the value for the button to Connect/Disconnect Google drive
   * depending on the current state.
   */
  private getConnectDisconnectButtonLabel_(): string {
    return this.driveDisabled_ ?
        this.i18n('googleDriveConnectLabel') :
        this.i18n('googleDriveRemoveDriveAccessButtonText');
  }

  /**
   * Returns the text representation of the total pinned size.
   */
  private getPinnedSizeLabel_(): string {
    if (this.totalPinnedSize_ === PinnedSizeType.CALCULATING) {
      return this.i18n('googleDriveOfflineClearCalculatingSubtitle');
    } else if (this.totalPinnedSize_ === PinnedSizeType.UNKNOWN) {
      return this.i18n('googleDriveOfflineClearErrorSubtitle');
    }

    return this.i18n(
        'googleDriveOfflineStorageSpaceTaken', this.totalPinnedSize_);
  }

  /**
   * Returns the text representation of the tooltip text when the "Clean up
   * storage" button is disabled.
   */
  private getCleanUpStorageDisabledTooltipText_(): string {
    if (this.totalPinnedSize_ === PinnedSizeType.UNKNOWN ||
        this.totalPinnedSize_ === PinnedSizeType.CALCULATING) {
      return this.i18n(
          'googleDriveCleanUpStorageDisabledUnknownStorageTooltip');
    }

    return this.i18n('googleDriveCleanUpStorageDisabledTooltip');
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
        this.setPrefValue(GOOGLE_DRIVE_BULK_PINNING_PREF, false);
        break;
      case ConfirmationDialogType.BULK_PINNING_DISABLE:
        this.setPrefValue(GOOGLE_DRIVE_BULK_PINNING_PREF, false);
        break;
      case ConfirmationDialogType.BULK_PINNING_CLEAN_UP_STORAGE:
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
      return this.i18n('googleDriveFileSyncSubtitleWithoutStorage');
    }

    const {requiredSpace, freeSpace} = this.bulkPinningStatus_;
    return this.i18n(
        'googleDriveFileSyncSubtitleWithStorage',
        requiredSpace!,
        freeSpace!,
    );
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

    // When the device is offline, don't allow the user to enable the toggle.
    if (this.bulkPinningStatus_?.stage === Stage.kPausedOffline) {
      target.checked = false;
      this.dialogType_ = ConfirmationDialogType.BULK_PINNING_OFFLINE;
      return;
    }

    // If currently enumerating the files, don't allow the user to enable file
    // sync until we're certain the corpus will fit on the device.
    if (this.bulkPinningStatus_?.stage === Stage.kListingFiles) {
      target.checked = false;
      this.dialogType_ = ConfirmationDialogType.BULK_PINNING_LISTING_FILES;
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
    this.proxy_.handler.recordBulkPinningEnabledMetric();
  }

  /**
   * Returns true if the bulk pinning preference is disabled.
   */
  private shouldEnableCleanUpStorageButton_() {
    return !this.getPref(GOOGLE_DRIVE_BULK_PINNING_PREF).value &&
        this.totalPinnedSize_ !== PinnedSizeType.UNKNOWN &&
        this.totalPinnedSize_ !== PinnedSizeType.CALCULATING;
  }

  /**
   * Returns the string used in the confirmation dialog when cleaning the users
   * offline storage, this includes the total GB used by offline files.
   */
  private getCleanUpStorageConfirmationDialogBody() {
    return this.i18nAdvanced('googleDriveOfflineCleanStorageDialogBody', {
      tags: ['a'],
      substitutions: [
        this.totalPinnedSize_!,
        this.i18n('googleDriveCleanUpStorageLearnMoreLink'),
      ],
    });
  }

  private getListingFilesDialogBody_() {
    return this.listedFiles_ > 0n ?
        this.i18n(
            'googleDriveFileSyncListingFilesItemsFoundBody',
            this.listedFiles_.toLocaleString()) :
        this.i18n('googleDriveFileSyncListingFilesBody');
  }

  /**
   * When the "Clean up storage" button is clicked, should not clean up
   * immediately but show the confirmation dialog first.
   */
  private async onCleanUpStorage_() {
    this.dialogType_ = ConfirmationDialogType.BULK_PINNING_CLEAN_UP_STORAGE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-google-drive-subpage': SettingsGoogleDriveSubpageElement;
  }
}

customElements.define(
    SettingsGoogleDriveSubpageElement.is, SettingsGoogleDriveSubpageElement);
