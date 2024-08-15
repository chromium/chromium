// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-nearby-share-subpage' is the settings subpage for managing the
 * Nearby Share feature.
 */

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './nearby_share_contact_visibility_dialog.js';
import './nearby_share_device_name_dialog.js';
import './nearby_share_data_usage_dialog.js';
import './nearby_share_receive_dialog.js';

import {getContactManager} from '/shared/nearby_contact_manager.js';
import {ReceiveManagerInterface, ReceiveObserverReceiver, ShareTarget, TransferMetadata} from '/shared/nearby_share.mojom-webui.js';
import {NearbySettings} from '/shared/nearby_share_settings_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DataUsage, FastInitiationNotificationState, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {NearbyAccountManagerBrowserProxyImpl} from './nearby_account_manager_browser_proxy.js';
import {NearbyShareReceiveDialogElement} from './nearby_share_receive_dialog.js';
import {getReceiveManager, observeReceiveManager} from './nearby_share_receive_manager.js';
import {getTemplate} from './nearby_share_subpage.html.js';
import {dataUsageStringToEnum} from './types.js';

const DEFAULT_HIGH_VISIBILITY_TIMEOUT_S = 300;

const SettingsNearbyShareSubpageElementBase =
    DeepLinkingMixin(PrefsMixin(RouteObserverMixin(I18nMixin(PolymerElement))));

export class SettingsNearbyShareSubpageElement extends
    SettingsNearbyShareSubpageElementBase {
  static get is() {
    return 'settings-nearby-share-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      profileName_: {
        type: String,
        value: '',
      },

      profileLabel_: {
        type: String,
        value: '',
      },

      settings: {
        type: Object,
        notify: true,
        value: {},
      },

      isSettingsRetreived: {
        type: Boolean,
        value: false,
      },

      showDeviceNameDialog_: {
        type: Boolean,
        value: false,
      },

      showVisibilityDialog_: {
        type: Boolean,
        value: false,
      },

      showDataUsageDialog_: {
        type: Boolean,
        value: false,
      },

      showReceiveDialog_: {
        type: Boolean,
        value: false,
      },

      manageContactsUrl_: {
        type: String,
        value: () => loadTimeData.getString('nearbyShareManageContactsUrl'),
      },

      inHighVisibility_: {
        type: Boolean,
        value: false,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kNearbyShareOnOff,
          Setting.kNearbyShareDeviceName,
          Setting.kNearbyShareDeviceVisibility,
          Setting.kNearbyShareContacts,
          Setting.kNearbyShareDataUsage,
          Setting.kDevicesNearbyAreSharingNotificationOnOff,
        ]),
      },

      shouldShowFastInititationNotificationToggle_: {
        type: Boolean,
        computed: `computeShouldShowFastInititationNotificationToggle_(
                settings.isFastInitiationHardwareSupported)`,
      },

      /**
       * Determines whether the QuickShareV2 flag is enabled.
       */
      isQuickShareV2Enabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isQuickShareV2Enabled'),
      },

      isDeviceVisible_: {
        type: Boolean,
        value: true,  // Correctly populated on settings load.
      },

      selectedVisibilityLabel_: {
        type: String,
        value: '',  // Populated on settings load.
      },

      isEveryoneModeOnlyForTenMinutes_: {
        type: Boolean,
        value: true,
      },

      yourDevicesLabel_: {
        type: String,
        value: 'Your devices',
      },

      contactsLabel_: {
        type: String,
        value: 'Contacts',
      },

      everyoneLabel_: {
        type: String,
        value: 'Everyone',
      },

      yourDevicesSublabel_: {
        type: String,
        computed: 'getYourDevicesVisibilitySublabel_(profileLabel_)',
      },

      isChangingHighVisibilityStatus_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'enabledChange_(settings.enabled)',
      'setSettingsVisibilityMenu_(settings.visibility)',
    ];
  }

  isSettingsRetreived: boolean;
  settings: NearbySettings;
  private isChangingHighVisibilityStatus_: boolean;
  private isDeviceVisible_: boolean;
  private isEveryoneModeOnlyForTenMinutes_: boolean;
  private inHighVisibility_: boolean;
  private isQuickShareV2Enabled_: boolean;
  private manageContactsUrl_: string;
  private profileLabel_: string;
  private profileName_: string;
  private receiveManager_: ReceiveManagerInterface|null;
  private receiveObserver_: ReceiveObserverReceiver|null;
  private selectedVisibilityLabel_: string;
  private shouldShowFastInititationNotificationToggle_: boolean;
  private showDataUsageDialog_: boolean;
  private showDeviceNameDialog_: boolean;
  private showReceiveDialog_: boolean;
  private showVisibilityDialog_: boolean;
  private yourDevicesLabel_: string;
  private yourDevicesSublabel_: string;
  private contactsLabel_: string;
  private everyoneLabel_: string;

  constructor() {
    super();

    this.receiveManager_ = null;
    this.receiveObserver_ = null;
  }

  override ready(): void {
    super.ready();

    this.addEventListener('onboarding-cancelled', this.onOnboardingCancelled_);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    // TODO(b/166779043): Check whether the Account Manager is enabled and fall
    // back to profile name, or just hide the row. This is not urgent because
    // the Account Manager should be available whenever Nearby Share is enabled.
    NearbyAccountManagerBrowserProxyImpl.getInstance().getAccounts().then(
        accounts => {
          if (accounts.length === 0) {
            return;
          }

          this.profileName_ = accounts[0].fullName;
          this.profileLabel_ = accounts[0].email;
        });
    this.receiveManager_ = getReceiveManager();
    this.receiveObserver_ = observeReceiveManager(this);
  }

  private enabledChange_(newValue: boolean, oldValue: boolean|undefined): void {
    if (oldValue === undefined && newValue) {
      // Trigger a contact sync whenever the Nearby subpage is opened and
      // nearby is enabled complete to improve consistency. This should help
      // avoid scenarios where a share is attempted and contacts are stale on
      // the receiver.
      getContactManager().downloadContacts();
    }
  }

  private onDeviceNameClick_(): void {
    if (this.showDeviceNameDialog_) {
      return;
    }
    this.showDeviceNameDialog_ = true;
  }

  private onVisibilityClick_(): void {
    this.showVisibilityDialog_ = true;
  }

  private onDataUsageClick_(): void {
    this.showDataUsageDialog_ = true;
  }

  private onDeviceNameDialogClose_(): void {
    this.showDeviceNameDialog_ = false;
  }

  private onVisibilityDialogClose_(): void {
    this.showVisibilityDialog_ = false;
  }

  private onDataUsageDialogClose_(): void {
    this.showDataUsageDialog_ = false;
  }

  private onReceiveDialogClose_(): void {
    this.showReceiveDialog_ = false;
    this.inHighVisibility_ = false;
  }

  private onManageContactsClick_(): void {
    window.open(this.manageContactsUrl_);
  }

  private getManageContactsSubLabel_(): string {
    // Remove the protocol part of the contacts url.
    return this.manageContactsUrl_.replace(/(^\w+:|^)\/\//, '');
  }

  /**
   * Mojo callback when high visibility changes.
   */
  onHighVisibilityChanged(inHighVisibility: boolean): void {
    this.inHighVisibility_ = inHighVisibility;

    if (!this.isQuickShareV2Enabled_) {
      return;
    }

    //  When Quick Share v2 is enabled, ensure that `setSettingsVisibilityMenu_`
    //  is not called when `this` is in the process of changing the high
    //  visibility status (un/register receive surface + execute callback).
    if (this.isChangingHighVisibilityStatus_) {
      return;
    }

    this.setSettingsVisibilityMenu_();
  }

  /**
   * Mojo callback when transfer status changes.
   */
  onTransferUpdate(_shareTarget: ShareTarget, _metadata: TransferMetadata):
      void {
    // Note: Intentionally left empty.
  }

  /**
   * Mojo callback when the Nearby utility process stops.
   */
  onNearbyProcessStopped(): void {
    this.inHighVisibility_ = false;
  }

  /**
   * Mojo callback when advertising fails to start.
   */
  onStartAdvertisingFailure(): void {
    this.inHighVisibility_ = false;
  }

  private onInHighVisibilityToggledByUser_(): void {
    if (this.inHighVisibility_) {
      this.showHighVisibilityPage_();
    }
  }

  /**
   * @param state boolean state that determines which string to show
   * @param onstr string to show when state is true
   * @param offstr string to show when state is false
   * @return localized string
   */
  private getOnOffString_(state: boolean, onstr: string, offstr: string):
      string {
    return state ? onstr : offstr;
  }

  private getEditNameButtonAriaDescription_(name: string): string {
    return this.i18n('nearbyShareDeviceNameAriaDescription', name);
  }

  private getVisibilityText_(visibility: Visibility|undefined): string {
    if (visibility === undefined) {
      return '';
    }
    switch (visibility) {
      case Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAll');
      case Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySome');
      case Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNone');
      case Visibility.kUnknown:
        return this.i18n('nearbyShareContactVisibilityUnknown');
      case Visibility.kYourDevices:
        return this.i18n('nearbyShareContactVisibilityYourDevices');
      default:
        assertNotReached();
    }
  }

  private getVisibilityDescription_(visibility: Visibility|undefined): string {
    if (visibility === undefined) {
      return '';
    }
    switch (visibility) {
      case Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAllDescription');
      case Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySomeDescription');
      case Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNoneDescription');
      case Visibility.kUnknown:
        return this.i18n('nearbyShareContactVisibilityUnknownDescription');
      case Visibility.kYourDevices:
        return this.i18n('nearbyShareContactVisibilityYourDevicesDescription');
      default:
        assertNotReached();
    }
  }

  private getHighVisibilityToggleText_(inHighVisibility: boolean): TrustedHTML
      |string {
    // TODO(crbug.com/40159645): Add logic to show how much time the user
    // actually has left.
    return inHighVisibility ?
        this.i18n('nearbyShareHighVisibilityOn', 5) :
        this.i18nAdvanced(
            'nearbyShareHighVisibilityOff', {substitutions: ['5']});
  }

  private getDataUsageLabel_(dataUsageValue: string): string {
    if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOnline) {
      return this.i18n('nearbyShareDataUsageDataLabel');
    } else if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOffline) {
      return this.i18n('nearbyShareDataUsageOfflineLabel');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyLabel');
    }
  }

  private getDataUsageSubLabel_(dataUsageValue: string): string {
    if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOnline) {
      return this.i18n('nearbyShareDataUsageDataDescription');
    } else if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOffline) {
      return this.i18n('nearbyShareDataUsageOfflineDescription');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyDescription');
    }
  }

  private getEditDataUsageButtonAriaDescription_(dataUsageValue: string):
      string {
    if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOnline) {
      return this.i18n('nearbyShareDataUsageDataEditButtonDescription');
    } else if (dataUsageStringToEnum(dataUsageValue) === DataUsage.kOffline) {
      return this.i18n('nearbyShareDataUsageOfflineEditButtonDescription');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyEditButtonDescription');
    }
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.NEARBY_SHARE) {
      return;
    }

    const router = Router.getInstance();
    const queryParams = router.getQueryParameters();

    if (queryParams.has('deviceName')) {
      this.showDeviceNameDialog_ = true;
    }

    if (queryParams.has('visibility')) {
      this.showVisibilityDialog_ = true;
    }

    if (queryParams.has('receive')) {
      this.showHighVisibilityPage_(Number(queryParams.get('timeout')));
    }

    if (queryParams.has('confirm')) {
      this.showReceiveDialog_ = true;
      flush();
      this.shadowRoot!
          .querySelector<NearbyShareReceiveDialogElement>(
              '#receiveDialog')!.showConfirmPage();
    }

    if (queryParams.has('onboarding')) {
      this.showOnboarding_();
    }

    this.attemptDeepLink();
  }

  private showHighVisibilityPage_(timeoutInSeconds?: number): void {
    const shutoffTimeoutInSeconds =
        timeoutInSeconds || DEFAULT_HIGH_VISIBILITY_TIMEOUT_S;
    this.showReceiveDialog_ = true;
    flush();
    this.shadowRoot!
        .querySelector<NearbyShareReceiveDialogElement>(
            '#receiveDialog')!.showHighVisibilityPage(shutoffTimeoutInSeconds);
  }

  private getAccountRowLabel(profileName: string, profileLabel: string):
      string {
    return this.i18n(
        'nearbyShareAccountRowLabel', this.i18n('nearbyShareFeatureName'),
        profileName, profileLabel);
  }

  private onOnboardingCancelled_(): void {
    // Return to main settings page multidevice section
    Router.getInstance().navigateTo(routes.MULTIDEVICE);
  }

  private onFastInitiationNotificationToggledByUser_(): void {
    this.set(
        'settings.fastInitiationNotificationState',
        this.isFastInitiationNotificationEnabled_() ?
            FastInitiationNotificationState.kDisabledByUser :
            FastInitiationNotificationState.kEnabled);
  }

  private isFastInitiationNotificationEnabled_(): boolean {
    return this.get('settings.fastInitiationNotificationState') ===
        FastInitiationNotificationState.kEnabled;
  }

  private shouldShowSubpageContent_(
      isNearbySharingEnabled: boolean, isOnboardingComplete: boolean,
      shouldShowFastInititationNotificationToggle: boolean): boolean {
    if (!isOnboardingComplete) {
      return false;
    }
    return isNearbySharingEnabled ||
        shouldShowFastInititationNotificationToggle;
  }

  private showOnboarding_(): void {
    this.showReceiveDialog_ = true;
    flush();
    this.shadowRoot!
        .querySelector<NearbyShareReceiveDialogElement>(
            '#receiveDialog')!.showOnboarding();
  }

  private computeShouldShowFastInititationNotificationToggle_(
      isHardwareSupported: boolean): boolean {
    return isHardwareSupported;
  }

  /**
   * Called when the Quick Share toggle is toggled.
   * - If Quick Share is toggled to on, set visibility to the previously
   * selected visibility if one exists
   * - If Quick Share is toggled to off, set visibility to NoOne
   */
  private onQuickShareVisibilityToggled_(): void {
    if (this.isDeviceVisible_) {
      this.isDeviceVisible_ = false;
      if (this.inHighVisibility_) {
        this.attemptSetVisibilityFromEveryoneVisibility_(Visibility.kNoOne);
        return;
      }

      this.setVisibility_(Visibility.kNoOne);
      return;
    }

    this.isDeviceVisible_ = true;
    switch (this.selectedVisibilityLabel_) {
      case this.everyoneLabel_:
        this.attemptSetEveryoneVisibility_(Visibility.kNoOne);
        break;
      case this.yourDevicesLabel_:
      case this.contactsLabel_:
        const newVisibility: Visibility =
            this.getVisibilityByLabel_(this.selectedVisibilityLabel_);
        this.setVisibility_(newVisibility);
        break;
      default:
        console.error(
            'Previously set visibility undetected or unset. Defaulting to Your devices visibility.');
        this.setVisibility_(Visibility.kYourDevices);
    }
  }

  /**
   * Called when user selects a new visibility in the visibility menu excluding
   * toggling Quick Share on/off. If Everyone is selected or deselected, attempt
   * to enable/disable high power scanning, respectively. Set the settings
   * visibility to the newly selected visibility.
   * @param e a CustomEvent containing a string, the label of the newly selected
   *     visibility.
   */
  private onSelectedVisibilityChange_(e: CustomEvent<{value: string}>): void {
    const newSelectedVisibilityLabel: string = e.detail.value;
    const currentSelectedVisibilityLabel: string =
        this.selectedVisibilityLabel_;

    if (newSelectedVisibilityLabel === this.everyoneLabel_) {
      this.attemptSetEveryoneVisibility_(this.settings.visibility);
      return;
    }

    const newSelectedVisibility: Visibility =
        this.getVisibilityByLabel_(newSelectedVisibilityLabel);
    if (currentSelectedVisibilityLabel === this.everyoneLabel_) {
      this.attemptSetVisibilityFromEveryoneVisibility_(newSelectedVisibility);
      return;
    }

    this.setVisibility_(newSelectedVisibility);
  }

  private setVisibility_(visibility: Visibility): void {
    this.set('settings.visibility', visibility);
  }

  private getVisibilityByLabel_(visibilityString: string): Visibility {
    switch (visibilityString) {
      case this.yourDevicesLabel_:
        return Visibility.kYourDevices;
      case this.contactsLabel_:
        return Visibility.kAllContacts;
      default:
        return Visibility.kUnknown;
    }
  }

  private isEveryoneModeSelected_(): boolean {
    return this.selectedVisibilityLabel_ === this.everyoneLabel_;
  }

  private getSelectedVisibility_(): Visibility|null {
    switch (this.selectedVisibilityLabel_) {
      case this.yourDevicesLabel_:
        return Visibility.kYourDevices;
      case this.contactsLabel_:
        return Visibility.kAllContacts;
      case this.everyoneLabel_:
        return Visibility.kAllContacts;
      default:
        return null;
    }
  }

  /**
   * Sets the toggle and checked states of objects in the settings menu
  according to visibility settings and whether the device is in high visibility
  mode. Called on change in settings visibility and change in high visibility
  status.
   */
  private setSettingsVisibilityMenu_(): void {
    if (!this.isQuickShareV2Enabled_) {
      return;
    }

    // Everyone visibility case. Since in the codebase 'Everyone' is a different
    // mode of advertisement and not an enumerated visibility (like Your
    // devices), it must be handled outside of the switch statement.
    if (this.inHighVisibility_) {
      this.isDeviceVisible_ = true;
      this.selectedVisibilityLabel_ = this.everyoneLabel_;
      return;
    }

    const visibility: Visibility = this.settings.visibility;
    switch (visibility) {
      case Visibility.kNoOne:
        this.isDeviceVisible_ = false;
        break;
      case Visibility.kAllContacts:
        this.isDeviceVisible_ = true;
        this.selectedVisibilityLabel_ = this.contactsLabel_;
        break;
      case Visibility.kSelectedContacts:
        // Selected contacts visibility does not exist in Quick Share v2. Set
        // visibility to Your devices.
        this.isDeviceVisible_ = true;
        this.setVisibility_(Visibility.kYourDevices);
        this.selectedVisibilityLabel_ = this.yourDevicesLabel_;
        break;
      case Visibility.kYourDevices:
        this.isDeviceVisible_ = true;
        this.selectedVisibilityLabel_ = this.yourDevicesLabel_;
        break;
      default:
        // If visibility is unset, default to Your devices.
        this.isDeviceVisible_ = true;
        this.setVisibility_(Visibility.kYourDevices);
        this.selectedVisibilityLabel_ = this.yourDevicesLabel_;
        break;
    }
  }

  private getYourDevicesVisibilitySublabel_(): TrustedHTML {
    return this.i18nAdvanced(
        'quickShareV2VisibilityYourDevicesSublabel',
        {substitutions: [this.profileLabel_]});
  }

  /**
   * Attempts to activate high power scanning. Called when the user has selected
   * Everyone visibility in the visibility menu.
   * - If attempt is unsuccessful, log error message
   * On success,
   * - If previously selected visibility is NoOne, set visibility to Your
   * devices
   * - Otherwise, restore the previously set visibility
   * @param previouslySetVisibility.
   */
  private attemptSetEveryoneVisibility_(previouslySetVisibility: Visibility):
      void {
    this.isChangingHighVisibilityStatus_ = true;
    this.activateHighPowerScanning_().then((result: number) => {
      // `result` = 0 indicates success, positive integers indicate failure.
      if (!result) {
        this.isDeviceVisible_ = true;
        this.selectedVisibilityLabel_ = this.everyoneLabel_;

        this.isChangingHighVisibilityStatus_ = false;
        return;
      }

      switch (previouslySetVisibility) {
        case Visibility.kAllContacts:
          this.setVisibility_(Visibility.kAllContacts);
          this.selectedVisibilityLabel_ = this.contactsLabel_;
          break;
        default:
          // All remaining cases fall to Your devices visibility:
          // - Your devices: trivial
          // - Selected contacts: doesn't exist in Quick Share v2, default to
          // most similar setting Your devices
          // - Unknown/NoOne: user wants to enable Quick Share, but Everyone
          // isn't available, default to most private visibility Your devices
          this.setVisibility_(Visibility.kYourDevices);
          this.selectedVisibilityLabel_ = this.yourDevicesLabel_;
      }

      this.isChangingHighVisibilityStatus_ = false;
    });
  }

  /**
   * Attempts to deactivate high power scanning. Called when user has selected
   * any visibility other than Everyone, or turned Quick Share off, and Everyone
   * is the previously selected visibility.
   * - If attempt is unsuccessful, keep visibility set to Everyone
   * On success,
   * - If the newly selected visibility is NoOne, turn Quick Share off,
   * `isDeviceVisible_` is set to false
   * - Otherwise, set visibility to `newVisibility`
   * @param newVisibility.
   */

  private attemptSetVisibilityFromEveryoneVisibility_(newVisibility:
                                                          Visibility): void {
    this.isChangingHighVisibilityStatus_ = true;
    this.deactivateHighPowerScanning_().then((success: boolean) => {
      if (success) {
        switch (newVisibility) {
          case Visibility.kAllContacts:
            this.setVisibility_(Visibility.kAllContacts);
            this.selectedVisibilityLabel_ = this.contactsLabel_;
            break;
          case Visibility.kNoOne:
            this.setVisibility_(Visibility.kNoOne);
            this.isDeviceVisible_ = false;
            break;
          default:
            this.setVisibility_(Visibility.kYourDevices);
            this.selectedVisibilityLabel_ = this.yourDevicesLabel_;
        }
        this.isChangingHighVisibilityStatus_ = false;
        return;
      }

      console.error('Unable to unregister Foreground receive surface.');
      if (newVisibility === Visibility.kNoOne) {
        this.isDeviceVisible_ = true;
      }
      this.selectedVisibilityLabel_ = this.everyoneLabel_;
      this.isChangingHighVisibilityStatus_ = false;
    });
  }

  /**
   * Register a foreground receive surface and returns a success status:
   * - 0 indicates success
   * - any positive integer indicates an error
   * @returns a Promise of a number, the success status of the attempt to
   *     register a foreground receive surface.
   */

  private async activateHighPowerScanning_(): Promise<number> {
    if (!this.receiveManager_) {
      console.error('Receive manager not connected.');
      return 1;
    }
    const callStatus: number =
        await this.receiveManager_!.registerForegroundReceiveSurface().then(
            (result) => {
              if (!result) {
                return 1;
              }
              return result.result;
            });

    return callStatus;
  }

  /**
   * Unregisters a foreground receive surface.
   * @returns a Promise of a boolean, indicates whether the foreground receive
   *     surface was successfully unregistered.
   */

  private async deactivateHighPowerScanning_(): Promise<boolean> {
    if (!this.receiveManager_) {
      console.error('Receive manager not connected.');
      return false;
    }

    const callSuccess: boolean =
        await this.receiveManager_!.unregisterForegroundReceiveSurface().then(
            (result) => {
              if (!result) {
                return false;
              }
              return result.success;
            });
    return callSuccess;
  }
}


declare global {
  interface HTMLElementTagNameMap {
    [SettingsNearbyShareSubpageElement.is]: SettingsNearbyShareSubpageElement;
  }
}

customElements.define(
    SettingsNearbyShareSubpageElement.is, SettingsNearbyShareSubpageElement);
