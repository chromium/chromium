// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-nearby-share-subpage' is the settings subpage for managing the
 * Nearby Share feature.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';
import './nearby_share_contact_visibility_dialog.js';
import './nearby_share_device_name_dialog.js';
import './nearby_share_data_usage_dialog.js';
import './nearby_share_receive_dialog.js';

import {ReceiveObserverReceiver, ShareTarget, TransferMetadata} from '/mojo/nearby_share.mojom-webui.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {FastInitiationNotificationState, Visibility} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {getContactManager} from '../../shared/nearby_contact_manager.js';
import {NearbySettings} from '../../shared/nearby_share_settings_mixin.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {NearbyAccountManagerBrowserProxyImpl} from './nearby_account_manager_browser_proxy.js';
import {NearbyShareReceiveDialogElement} from './nearby_share_receive_dialog.js';
import {observeReceiveManager} from './nearby_share_receive_manager.js';
import {getTemplate} from './nearby_share_subpage.html.js';
import {dataUsageStringToEnum, NearbyShareDataUsage} from './types.js';

const DEFAULT_HIGH_VISIBILITY_TIMEOUT_S: number = 300;

const SettingsNearbyShareSubpageElementBase =
    DeepLinkingMixin(PrefsMixin(RouteObserverMixin(I18nMixin(PolymerElement))));

class SettingsNearbyShareSubpageElement extends
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
    };
  }

  static get observers() {
    return ['enabledChange_(settings.enabled)'];
  }

  isSettingsRetreived: boolean;
  private inHighVisibility_: boolean;
  private manageContactsUrl_: string;
  private profileLabel_: string;
  private profileName_: string;
  private receiveObserver_: ReceiveObserverReceiver|null;
  private settings: NearbySettings;
  private shouldShowFastInititationNotificationToggle_: boolean;
  private showDataUsageDialog_: boolean;
  private showDeviceNameDialog_: boolean;
  private showReceiveDialog_: boolean;
  private showVisibilityDialog_: boolean;

  constructor() {
    super();

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

  private onDeviceNameTap_(): void {
    if (this.showDeviceNameDialog_) {
      return;
    }
    this.showDeviceNameDialog_ = true;
  }

  private onVisibilityTap_(): void {
    this.showVisibilityDialog_ = true;
  }

  private onDataUsageTap_(): void {
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

  private onManageContactsTap_(): void {
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
      default:
        assertNotReached();
    }
  }

  private getHighVisibilityToggleText_(inHighVisibility: boolean): TrustedHTML
      |string {
    // TODO(crbug.com/1154830): Add logic to show how much time the user
    // actually has left.
    return inHighVisibility ?
        this.i18n('nearbyShareHighVisibilityOn', 5) :
        this.i18nAdvanced(
            'nearbyShareHighVisibilityOff', {substitutions: ['5']});
  }

  private getDataUsageLabel_(dataUsageValue: string): string {
    if (dataUsageStringToEnum(dataUsageValue) === NearbyShareDataUsage.ONLINE) {
      return this.i18n('nearbyShareDataUsageDataLabel');
    } else if (
        dataUsageStringToEnum(dataUsageValue) ===
        NearbyShareDataUsage.OFFLINE) {
      return this.i18n('nearbyShareDataUsageOfflineLabel');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyLabel');
    }
  }

  private getDataUsageSubLabel_(dataUsageValue: string): string {
    if (dataUsageStringToEnum(dataUsageValue) === NearbyShareDataUsage.ONLINE) {
      return this.i18n('nearbyShareDataUsageDataDescription');
    } else if (
        dataUsageStringToEnum(dataUsageValue) ===
        NearbyShareDataUsage.OFFLINE) {
      return this.i18n('nearbyShareDataUsageOfflineDescription');
    } else {
      return this.i18n('nearbyShareDataUsageWifiOnlyDescription');
    }
  }

  private getEditDataUsageButtonAriaDescription_(dataUsageValue: string):
      string {
    if (dataUsageStringToEnum(dataUsageValue) === NearbyShareDataUsage.ONLINE) {
      return this.i18n('nearbyShareDataUsageDataEditButtonDescription');
    } else if (
        dataUsageStringToEnum(dataUsageValue) ===
        NearbyShareDataUsage.OFFLINE) {
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
    return this.i18n('nearbyShareAccountRowLabel', profileName, profileLabel);
  }

  private getEnabledToggleClassName_(): string {
    if (this.getPref('nearby_sharing.enabled').value) {
      return 'enabled-toggle-on';
    }
    return 'enabled-toggle-off';
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
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsNearbyShareSubpageElement.is]: SettingsNearbyShareSubpageElement;
  }
}

customElements.define(
    SettingsNearbyShareSubpageElement.is, SettingsNearbyShareSubpageElement);
