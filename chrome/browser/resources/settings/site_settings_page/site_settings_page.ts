// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 */

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './recent_site_permissions.js';
import './unused_site_permissions.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FocusConfig} from '../focus_config.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SafetyHubBrowserProxy, SafetyHubBrowserProxyImpl, SafetyHubEvent, UnusedSitePermissions} from '../safety_hub/safety_hub_browser_proxy.js';
import {ContentSettingsTypes} from '../site_settings/constants.js';

import {CategoryListItem} from './site_settings_list.js';
import {getTemplate} from './site_settings_page.html.js';

const Id = ContentSettingsTypes;

let categoryItemMap: Map<ContentSettingsTypes, CategoryListItem>|null = null;

function getCategoryItemMap(): Map<ContentSettingsTypes, CategoryListItem> {
  if (categoryItemMap !== null) {
    return categoryItemMap;
  }
  // The following list is ordered alphabetically by |id|. The order in which
  // these appear in the UI is determined elsewhere in this file.
  const categoryList = [
    {
      route: routes.SITE_SETTINGS_ADS,
      id: Id.ADS,
      label: 'siteSettingsAds',
      icon: 'settings:ads',
      enabledLabel: 'siteSettingsAdsAllowed',
      disabledLabel: 'siteSettingsAdsBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter'),
    },
    {
      route: routes.SITE_SETTINGS_AUTO_PICTURE_IN_PICTURE,
      id: Id.AUTO_PICTURE_IN_PICTURE,
      label: 'siteSettingsAutoPictureInPicture',
      // TODO(https://crbug.com/1471051): Use real icon.
      icon: 'settings:window-management',
      enabledLabel: 'siteSettingsAutoPictureInPictureAllowed',
      disabledLabel: 'siteSettingsAutoPictureInPictureBlocked',
      shouldShow: () => loadTimeData.getBoolean('autoPictureInPictureEnabled'),
    },
    {
      route: routes.SITE_SETTINGS_AUTO_VERIFY,
      id: Id.ANTI_ABUSE,
      label: 'siteSettingsAntiAbuse',
      icon: 'settings20:account-attention',
      enabledLabel: 'siteSettingsAntiAbuseEnabledSubLabel',
      disabledLabel: 'siteSettingsAntiAbuseDisabledSubLabel',
      shouldShow: () => loadTimeData.getBoolean('privateStateTokensEnabled'),
    },
    {
      route: routes.SITE_SETTINGS_AR,
      id: Id.AR,
      label: 'siteSettingsAr',
      icon: 'settings:vr-headset',
      // TODO(crbug.com/1196900): Fix redesign string when available.
      enabledLabel: 'siteSettingsArAsk',
      disabledLabel: 'siteSettingsArBlock',
    },
    {
      route: routes.SITE_SETTINGS_AUTOMATIC_DOWNLOADS,
      id: Id.AUTOMATIC_DOWNLOADS,
      label: 'siteSettingsAutomaticDownloads',
      icon: 'cr:file-download',
      enabledLabel: 'siteSettingsAutomaticDownloadsAllowed',
      disabledLabel: 'siteSettingsAutomaticDownloadsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_BACKGROUND_SYNC,
      id: Id.BACKGROUND_SYNC,
      label: 'siteSettingsBackgroundSync',
      icon: 'cr:sync',
      enabledLabel: 'siteSettingsBackgroundSyncAllowed',
      disabledLabel: 'siteSettingsBackgroundSyncBlocked',
    },
    {
      route: routes.SITE_SETTINGS_BLUETOOTH_DEVICES,
      id: Id.BLUETOOTH_DEVICES,
      label: 'siteSettingsBluetoothDevices',
      icon: 'settings:bluetooth',
      enabledLabel: 'siteSettingsBluetoothDevicesAllowed',
      disabledLabel: 'siteSettingsBluetoothDevicesBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend'),
    },
    {
      route: routes.SITE_SETTINGS_BLUETOOTH_SCANNING,
      id: Id.BLUETOOTH_SCANNING,
      label: 'siteSettingsBluetoothScanning',
      icon: 'settings:bluetooth-scanning',
      enabledLabel: 'siteSettingsBluetoothScanningAsk',
      disabledLabel: 'siteSettingsBluetoothScanningBlock',
      shouldShow: () =>
          loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures'),
    },
    {
      route: routes.SITE_SETTINGS_CAMERA,
      id: Id.CAMERA,
      label: 'siteSettingsCamera',
      icon: 'cr:videocam',
      enabledLabel: 'siteSettingsCameraAllowed',
      disabledLabel: 'siteSettingsCameraBlocked',
    },
    {
      route: routes.SITE_SETTINGS_CLIPBOARD,
      id: Id.CLIPBOARD,
      label: 'siteSettingsClipboard',
      icon: 'settings:clipboard',
      enabledLabel: 'siteSettingsClipboardAllowed',
      disabledLabel: 'siteSettingsClipboardBlocked',
    },
    {
      route: routes.COOKIES,
      id: Id.COOKIES,
      label: loadTimeData.getBoolean('isPrivacySandboxSettings4') ?
          'thirdPartyCookiesLinkRowLabel' :
          'siteSettingsCookies',
      icon: 'settings:cookie',
      enabledLabel: 'siteSettingsCookiesAllowed',
      disabledLabel: 'siteSettingsBlocked',
      otherLabel: 'cookiePageClearOnExit',
    },
    {
      route: routes.SITE_SETTINGS_LOCATION,
      id: Id.GEOLOCATION,
      label: 'siteSettingsLocation',
      icon: 'settings:location-on',
      enabledLabel: 'siteSettingsLocationAllowed',
      disabledLabel: 'siteSettingsLocationBlocked',
    },
    {
      route: routes.SITE_SETTINGS_HID_DEVICES,
      id: Id.HID_DEVICES,
      label: 'siteSettingsHidDevices',
      icon: 'settings:hid-device',
      enabledLabel: 'siteSettingsHidDevicesAsk',
      disabledLabel: 'siteSettingsHidDevicesBlock',
    },
    {
      route: routes.SITE_SETTINGS_IDLE_DETECTION,
      id: Id.IDLE_DETECTION,
      label: 'siteSettingsIdleDetection',
      icon: 'settings:devices',
      enabledLabel: 'siteSettingsDeviceUseAllowed',
      disabledLabel: 'siteSettingsDeviceUseBlocked',
    },
    {
      route: routes.SITE_SETTINGS_IMAGES,
      id: Id.IMAGES,
      label: 'siteSettingsImages',
      icon: 'settings:photo',
      enabledLabel: 'siteSettingsImagesAllowed',
      disabledLabel: 'siteSettingsImagesBlocked',
    },
    {
      route: routes.SITE_SETTINGS_JAVASCRIPT,
      id: Id.JAVASCRIPT,
      label: 'siteSettingsJavascript',
      icon: 'settings:code',
      enabledLabel: 'siteSettingsJavascriptAllowed',
      disabledLabel: 'siteSettingsJavascriptBlocked',
    },
    {
      route: routes.SITE_SETTINGS_MICROPHONE,
      id: Id.MIC,
      label: 'siteSettingsMic',
      icon: 'cr:mic',
      enabledLabel: 'siteSettingsMicAllowed',
      disabledLabel: 'siteSettingsMicBlocked',
    },
    {
      route: routes.SITE_SETTINGS_MIDI_DEVICES,
      id: Id.MIDI_DEVICES,
      label: 'siteSettingsMidiDevices',
      icon: 'settings:midi',
      enabledLabel: 'siteSettingsMidiAllowed',
      disabledLabel: 'siteSettingsMidiBlocked',
    },
    {
      route: routes.SITE_SETTINGS_MIXEDSCRIPT,
      id: Id.MIXEDSCRIPT,
      label: 'siteSettingsInsecureContent',
      icon: 'settings:insecure-content',
      disabledLabel: 'siteSettingsInsecureContentBlock',
    },
    {
      route: routes.SITE_SETTINGS_FEDERATED_IDENTITY_API,
      id: Id.FEDERATED_IDENTITY_API,
      label: 'siteSettingsFederatedIdentityApi',
      icon: 'settings:federated-identity-api',
      enabledLabel: 'siteSettingsFederatedIdentityApiAllowed',
      disabledLabel: 'siteSettingsFederatedIdentityApiBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableFederatedIdentityApiContentSetting'),
    },
    {
      route: routes.SITE_SETTINGS_FILE_SYSTEM_WRITE,
      id: Id.FILE_SYSTEM_WRITE,
      label: 'siteSettingsFileSystemWrite',
      icon: 'settings:save-original',
      enabledLabel: 'siteSettingsFileSystemWriteAllowed',
      disabledLabel: 'siteSettingsFileSystemWriteBlocked',
    },
    {
      route: routes.SITE_SETTINGS_LOCAL_FONTS,
      id: Id.LOCAL_FONTS,
      label: 'fonts',
      icon: 'settings:local-fonts',
      enabledLabel: 'siteSettingsFontsAllowed',
      disabledLabel: 'siteSettingsFontsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_NOTIFICATIONS,
      id: Id.NOTIFICATIONS,
      label: 'siteSettingsNotifications',
      icon: 'settings:notifications',
      enabledLabel: 'siteSettingsAskBeforeSending',
      disabledLabel: 'siteSettingsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_PAYMENT_HANDLER,
      id: Id.PAYMENT_HANDLER,
      label: 'siteSettingsPaymentHandler',
      icon: 'settings:payment-handler',
      enabledLabel: 'siteSettingsPaymentHandlersAllowed',
      disabledLabel: 'siteSettingsPaymentHandlersBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enablePaymentHandlerContentSetting'),
    },
    {
      route: routes.SITE_SETTINGS_PDF_DOCUMENTS,
      id: Id.PDF_DOCUMENTS,
      label: 'siteSettingsPdfDocuments',
      icon: 'settings:pdf',
      enabledLabel: 'siteSettingsPdfsAllowed',
      disabledLabel: 'siteSettingsPdfsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_POPUPS,
      id: Id.POPUPS,
      label: 'siteSettingsPopups',
      icon: 'cr:open-in-new',
      enabledLabel: 'siteSettingsPopupsAllowed',
      disabledLabel: 'siteSettingsPopupsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_PROTECTED_CONTENT,
      id: Id.PROTECTED_CONTENT,
      label: 'siteSettingsProtectedContent',
      icon: 'settings:protected-content',
      enabledLabel: 'siteSettingsProtectedContentAllowed',
      disabledLabel: 'siteSettingsProtectedContentBlocked',
    },
    {
      route: routes.SITE_SETTINGS_HANDLERS,
      id: Id.PROTOCOL_HANDLERS,
      label: 'siteSettingsHandlers',
      icon: 'settings:protocol-handler',
      enabledLabel: 'siteSettingsProtocolHandlersAllowed',
      disabledLabel: 'siteSettingsProtocolHandlersBlocked',
      shouldShow: () => !loadTimeData.getBoolean('isGuest'),
    },
    {
      route: routes.SITE_SETTINGS_SENSORS,
      id: Id.SENSORS,
      label: 'siteSettingsSensors',
      icon: 'settings:sensors',
      enabledLabel: 'siteSettingsMotionSensorsAllowed',
      disabledLabel: 'siteSettingsMotionSensorsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_SERIAL_PORTS,
      id: Id.SERIAL_PORTS,
      label: 'siteSettingsSerialPorts',
      icon: 'settings:serial-port',
      enabledLabel: 'siteSettingsSerialPortsAllowed',
      disabledLabel: 'siteSettingsSerialPortsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_SITE_DATA,
      id: Id.SITE_DATA,
      label: 'siteDataPageTitle',
      icon: 'settings:database',
      shouldShow: () => loadTimeData.getBoolean('isPrivacySandboxSettings4'),
    },
    {
      route: routes.SITE_SETTINGS_SOUND,
      id: Id.SOUND,
      label: 'siteSettingsSound',
      icon: 'settings:volume-up',
      enabledLabel: 'siteSettingsSoundAllowed',
      disabledLabel: 'siteSettingsSoundBlocked',
    },
    {
      route: routes.SITE_SETTINGS_STORAGE_ACCESS,
      id: Id.STORAGE_ACCESS,
      label: 'siteSettingsStorageAccess',
      icon: 'settings:storage-access',
      enabledLabel: 'storageAccessAllowed',
      disabledLabel: 'storageAccessBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enablePermissionStorageAccessApi'),
    },
    {
      route: routes.SITE_SETTINGS_USB_DEVICES,
      id: Id.USB_DEVICES,
      label: 'siteSettingsUsbDevices',
      icon: 'settings:usb',
      enabledLabel: 'siteSettingsUsbAllowed',
      disabledLabel: 'siteSettingsUsbBlocked',
    },
    {
      route: routes.SITE_SETTINGS_VR,
      id: Id.VR,
      label: 'siteSettingsVr',
      icon: 'settings:vr-headset',
      enabledLabel: 'siteSettingsVrAllowed',
      disabledLabel: 'siteSettingsVrBlocked',
    },
    {
      route: routes.SITE_SETTINGS_WINDOW_MANAGEMENT,
      id: Id.WINDOW_MANAGEMENT,
      label: 'siteSettingsWindowManagement',
      icon: 'settings:window-management',
      enabledLabel: 'siteSettingsWindowManagementAsk',
      disabledLabel: 'siteSettingsWindowManagementBlocked',
    },
    {
      route: routes.SITE_SETTINGS_ZOOM_LEVELS,
      id: Id.ZOOM_LEVELS,
      label: 'siteSettingsZoomLevels',
      icon: 'settings:zoom-in',
    },
  ];

  categoryItemMap = new Map(categoryList.map(item => [item.id, item]));
  return categoryItemMap;
}

function buildItemListFromIds(orderedIdList: ContentSettingsTypes[]):
    CategoryListItem[] {
  const map = getCategoryItemMap();
  const orderedList = [];
  for (const id of orderedIdList) {
    const item = map.get(id)!;
    if (item.shouldShow === undefined || item.shouldShow()) {
      orderedList.push(item);
    }
  }
  return orderedList;
}

export interface SettingsSiteSettingsPageElement {
  $: {
    advancedContentList: HTMLElement,
  };
}

const SettingsSiteSettingsPageElementBase = WebUiListenerMixin(PolymerElement);

export class SettingsSiteSettingsPageElement extends
    SettingsSiteSettingsPageElementBase {
  static get is() {
    return 'settings-site-settings-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      lists_: {
        type: Object,
        value: function() {
          // Move `BACKGROUND_SYNC` to the sixth position under the fold if
          // `STORAGE_ACCESS` is present.
          const enablePermissionStorageAccessApi =
              loadTimeData.getBoolean('enablePermissionStorageAccessApi');
          const basic = enablePermissionStorageAccessApi ? Id.STORAGE_ACCESS :
                                                           Id.BACKGROUND_SYNC;
          const advanced: ContentSettingsTypes[] =
              enablePermissionStorageAccessApi ? [Id.BACKGROUND_SYNC] : [];

          return {
            permissionsBasic: buildItemListFromIds([
              Id.GEOLOCATION,
              Id.CAMERA,
              Id.MIC,
              Id.NOTIFICATIONS,
              basic,
            ]),
            permissionsAdvanced: buildItemListFromIds([
              ...advanced,
              ...[Id.SENSORS,
                  Id.AUTOMATIC_DOWNLOADS,
                  Id.PROTOCOL_HANDLERS,
                  Id.MIDI_DEVICES,
                  Id.USB_DEVICES,
                  Id.SERIAL_PORTS,
                  Id.BLUETOOTH_DEVICES,
                  Id.FILE_SYSTEM_WRITE,
                  Id.HID_DEVICES,
                  Id.CLIPBOARD,
                  Id.PAYMENT_HANDLER,
                  Id.BLUETOOTH_SCANNING,
                  Id.AR,
                  Id.VR,
                  Id.IDLE_DETECTION,
                  Id.WINDOW_MANAGEMENT,
                  Id.LOCAL_FONTS,
                  Id.AUTO_PICTURE_IN_PICTURE,
          ],
            ]),
            contentBasic: buildItemListFromIds([
              Id.COOKIES,
              Id.JAVASCRIPT,
              Id.IMAGES,
              Id.POPUPS,
            ]),
            contentAdvanced: buildItemListFromIds([
              Id.SOUND,
              Id.ADS,
              Id.ZOOM_LEVELS,
              Id.PDF_DOCUMENTS,
              Id.PROTECTED_CONTENT,
              Id.MIXEDSCRIPT,
              Id.FEDERATED_IDENTITY_API,
              Id.ANTI_ABUSE,
              Id.SITE_DATA,
            ]),
          };
        },
      },

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      permissionsExpanded_: Boolean,
      contentExpanded_: Boolean,
      noRecentSitePermissions_: Boolean,

      showUnusedSitePermissions_: {
        type: Boolean,
        value: false,
      },

      unusedSitePermissionsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'safetyCheckUnusedSitePermissionsEnabled');
        },
      },

      enableSafetyHub_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSafetyHub');
        },
      },
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));

    this.safetyHubBrowserProxy_.getRevokedUnusedSitePermissionsList().then(
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));
  }

  prefs: Object;
  focusConfig: FocusConfig;
  private permissionsExpanded_: boolean;
  private contentExpanded_: boolean;
  private noRecentSitePermissions_: boolean;
  private showUnusedSitePermissions_: boolean;
  private unusedSitePermissionsEnabled_: boolean;
  private safetyHubBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();

  private lists_: {
    all: CategoryListItem[],
    permissionsBasic: CategoryListItem[],
    permissionsAdvanced: CategoryListItem[],
    contentBasic: CategoryListItem[],
    contentAdvanced: CategoryListItem[],
  };

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);
    this.focusConfig.set(routes.SITE_SETTINGS_ALL.path, () => {
      const allSites = this.shadowRoot!.querySelector<HTMLElement>('#allSites');
      assert(!!allSites);
      focusWithoutInk(allSites);
    });
  }

  private onSiteSettingsAllClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
  }

  private onUnusedSitePermissionListChanged_(permissions:
                                                 UnusedSitePermissions[]) {
    // The unused site permissions review is shown when there are items to
    // review (provided the feature is enabled). Once visible it remains that
    // way to show completion info, even if the list is emptied.
    if (this.showUnusedSitePermissions_) {
      return;
    }

    this.showUnusedSitePermissions_ = this.unusedSitePermissionsEnabled_ &&
        permissions.length > 0 && !loadTimeData.getBoolean('isGuest');
  }

  /** @return Class for the all site settings link */
  private getClassForSiteSettingsAllLink_(): string {
    return this.noRecentSitePermissions_ ? '' : 'hr';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-site-settings-page': SettingsSiteSettingsPageElement;
  }
}

customElements.define(
    SettingsSiteSettingsPageElement.is, SettingsSiteSettingsPageElement);
