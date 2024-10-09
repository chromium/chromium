// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 */

import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../controls/settings_toggle_button.js';
import '../privacy_icons.html.js';
import '../settings_shared.css.js';
import './recent_site_permissions.js';
import './unused_site_permissions.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {FocusConfig} from '../focus_config.js';
import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyHubEntryPoint} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {SafetyHubBrowserProxy, UnusedSitePermissions} from '../safety_hub/safety_hub_browser_proxy.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from '../safety_hub/safety_hub_browser_proxy.js';
import {ContentSettingsTypes} from '../site_settings/constants.js';

import type {CategoryListItem} from './site_settings_list.js';
import {getTemplate} from './site_settings_page.html.js';

const Id = ContentSettingsTypes;

let categoryItemMap: Map<ContentSettingsTypes, CategoryListItem>|null = null;

function getCategoryItemMap(): Map<ContentSettingsTypes, CategoryListItem> {
  if (categoryItemMap !== null) {
    return categoryItemMap;
  }
  // The following list is ordered alphabetically by |id|. The order in which
  // these appear in the UI is determined elsewhere in this file.
  const categoryList: CategoryListItem[] = [
    {
      route: routes.SITE_SETTINGS_ADS,
      id: Id.ADS,
      label: 'siteSettingsAds',
      icon: 'privacy:web-asset',
      enabledLabel: 'siteSettingsAdsAllowed',
      disabledLabel: 'siteSettingsAdsBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter'),
    },
    {
      route: routes.SITE_SETTINGS_AUTO_VERIFY,
      id: Id.ANTI_ABUSE,
      label: 'siteSettingsAntiAbuse',
      icon: 'privacy20:person-check',
      enabledLabel: 'siteSettingsAntiAbuseEnabledSubLabel',
      disabledLabel: 'siteSettingsAntiAbuseDisabledSubLabel',
      shouldShow: () => loadTimeData.getBoolean('privateStateTokensEnabled'),
    },
    {
      route: routes.SITE_SETTINGS_AR,
      id: Id.AR,
      label: 'siteSettingsAr',
      icon: 'privacy:cardboard',
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
      route: routes.SITE_SETTINGS_AUTOMATIC_FULLSCREEN,
      id: Id.AUTOMATIC_FULLSCREEN,
      label: 'siteSettingsAutomaticFullscreen',
      icon: 'cr:fullscreen',
      shouldShow: () =>
          loadTimeData.getBoolean('enableAutomaticFullscreenContentSetting'),
    },
    {
      route: routes.SITE_SETTINGS_AUTO_PICTURE_IN_PICTURE,
      id: Id.AUTO_PICTURE_IN_PICTURE,
      label: 'siteSettingsAutoPictureInPicture',
      icon: 'settings:picture-in-picture',
      enabledLabel: 'siteSettingsAutoPictureInPictureAllowed',
      disabledLabel: 'siteSettingsAutoPictureInPictureBlocked',
      shouldShow: () => loadTimeData.getBoolean('autoPictureInPictureEnabled'),
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
      route: routes.SITE_SETTINGS_CAPTURED_SURFACE_CONTROL,
      id: Id.CAPTURED_SURFACE_CONTROL,
      label: 'siteSettingsCapturedSurfaceControl',
      icon: 'settings:touchpad-mouse',
      enabledLabel: 'siteSettingsCapturedSurfaceControlAllowed',
      disabledLabel: 'siteSettingsCapturedSurfaceControlBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('capturedSurfaceControlEnabled'),
    },
    {
      route: routes.SITE_SETTINGS_CLIPBOARD,
      id: Id.CLIPBOARD,
      label: 'siteSettingsClipboard',
      icon: 'privacy:content-paste',
      enabledLabel: 'siteSettingsClipboardAllowed',
      disabledLabel: 'siteSettingsClipboardBlocked',
    },
    {
      route: routes.SITE_SETTINGS_FEDERATED_IDENTITY_API,
      id: Id.FEDERATED_IDENTITY_API,
      label: 'siteSettingsFederatedIdentityApi',
      icon: 'privacy:account-circle',
      enabledLabel: 'siteSettingsFederatedIdentityApiAllowed',
      disabledLabel: 'siteSettingsFederatedIdentityApiBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableFederatedIdentityApiContentSetting'),
    },
    {
      route: routes.SITE_SETTINGS_FILE_SYSTEM_WRITE,
      id: Id.FILE_SYSTEM_WRITE,
      label: 'siteSettingsFileSystemWrite',
      icon: 'privacy:file-save',
      enabledLabel: 'siteSettingsFileSystemWriteAllowed',
      disabledLabel: 'siteSettingsFileSystemWriteBlocked',
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
      route: routes.SITE_SETTINGS_HAND_TRACKING,
      id: Id.HAND_TRACKING,
      label: 'siteSettingsHandTracking',
      icon: 'privacy:hand-gesture',
      enabledLabel: 'siteSettingsHandTrackingAsk',
      disabledLabel: 'siteSettingsHandTrackingBlock',
      shouldShow: () =>
          loadTimeData.getBoolean('enableHandTrackingContentSetting'),
    },
    {
      route: routes.SITE_SETTINGS_HID_DEVICES,
      id: Id.HID_DEVICES,
      label: 'siteSettingsHidDevices',
      icon: 'privacy:videogame-asset',
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
      icon: 'privacy:imagesmode',
      enabledLabel: 'siteSettingsImagesAllowed',
      disabledLabel: 'siteSettingsImagesBlocked',
    },
    {
      route: routes.SITE_SETTINGS_JAVASCRIPT,
      id: Id.JAVASCRIPT,
      label: 'siteSettingsJavascript',
      icon: 'privacy:code',
      enabledLabel: 'siteSettingsJavascriptAllowed',
      disabledLabel: 'siteSettingsJavascriptBlocked',
    },
    {
      route: routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER,
      id: Id.JAVASCRIPT_OPTIMIZER,
      label: 'siteSettingsJavascriptOptimizer',
      icon: 'privacy:v8',
      enabledLabel: 'siteSettingsJavascriptOptimizerAllowed',
      disabledLabel: 'siteSettingsJavascriptOptimizerBlocked',
    },
    {
      route: routes.SITE_SETTINGS_KEYBOARD_LOCK,
      id: Id.KEYBOARD_LOCK,
      label: 'siteSettingsKeyboardLock',
      icon: 'settings20:keyboard-lock',
      enabledLabel: 'siteSettingsKeyboardLockAllowed',
      disabledLabel: 'siteSettingsKeyboardLockBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableKeyboardAndPointerLockPrompt'),
    },
    {
      route: routes.SITE_SETTINGS_LOCAL_FONTS,
      id: Id.LOCAL_FONTS,
      label: 'fonts',
      icon: 'privacy:font-download',
      enabledLabel: 'siteSettingsFontsAllowed',
      disabledLabel: 'siteSettingsFontsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_MICROPHONE,
      id: Id.MIC,
      label: 'siteSettingsMic',
      icon: 'privacy:mic',
      enabledLabel: 'siteSettingsMicAllowed',
      disabledLabel: 'siteSettingsMicBlocked',
    },
    {
      route: routes.SITE_SETTINGS_MIDI_DEVICES,
      id: Id.MIDI_DEVICES,
      label: 'siteSettingsMidiDevices',
      icon: 'privacy:piano',
      enabledLabel: 'siteSettingsMidiAllowed',
      disabledLabel: 'siteSettingsMidiBlocked',
    },
    {
      route: routes.SITE_SETTINGS_MIXEDSCRIPT,
      id: Id.MIXEDSCRIPT,
      label: 'siteSettingsInsecureContent',
      icon: 'privacy:warning',
      disabledLabel: 'siteSettingsInsecureContentBlock',
    },
    {
      route: routes.SITE_SETTINGS_NOTIFICATIONS,
      id: Id.NOTIFICATIONS,
      label: 'siteSettingsNotifications',
      icon: 'privacy:notifications',
    },
    {
      route: routes.OFFER_WRITING_HELP,
      id: Id.OFFER_WRITING_HELP,
      label: 'siteSettingsOfferWritingHelp',
      icon: 'settings:compose',
      shouldShow: () => loadTimeData.getBoolean('enableComposeProactiveNudge'),
    },
    {
      route: routes.SITE_SETTINGS_PAYMENT_HANDLER,
      id: Id.PAYMENT_HANDLER,
      label: 'siteSettingsPaymentHandler',
      icon: 'privacy:credit-card',
      enabledLabel: 'siteSettingsPaymentHandlersAllowed',
      disabledLabel: 'siteSettingsPaymentHandlersBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enablePaymentHandlerContentSetting'),
    },
    {
      route: routes.SITE_SETTINGS_PDF_DOCUMENTS,
      id: Id.PDF_DOCUMENTS,
      label: 'siteSettingsPdfDocuments',
      icon: 'privacy:drive-pdf',
      enabledLabel: 'siteSettingsPdfsAllowed',
      disabledLabel: 'siteSettingsPdfsBlocked',
    },
    {
      route: routes.PERFORMANCE,
      id: Id.PERFORMANCE,
      label: 'siteSettingsPerformance',
      icon: 'settings:performance',
      enabledLabel: 'siteSettingsPerformanceSublabel',
      disabledLabel: 'siteSettingsPerformanceSublabel',
    },
    {
      route: routes.SITE_SETTINGS_POINTER_LOCK,
      id: Id.POINTER_LOCK,
      label: 'siteSettingsPointerLock',
      icon: 'settings20:pointer-lock',
      enabledLabel: 'siteSettingsPointerLockAllowed',
      disabledLabel: 'siteSettingsPointerLockBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableKeyboardAndPointerLockPrompt'),
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
      icon: 'privacy:sync-saved-locally',
      enabledLabel: 'siteSettingsProtectedContentAllowed',
      disabledLabel: 'siteSettingsProtectedContentBlocked',
    },
    {
      route: routes.SITE_SETTINGS_HANDLERS,
      id: Id.PROTOCOL_HANDLERS,
      label: 'siteSettingsHandlers',
      icon: 'privacy:protocol-handler',
      enabledLabel: 'siteSettingsProtocolHandlersAllowed',
      disabledLabel: 'siteSettingsProtocolHandlersBlocked',
      shouldShow: () => !loadTimeData.getBoolean('isGuest'),
    },
    {
      route: routes.SITE_SETTINGS_SENSORS,
      id: Id.SENSORS,
      label: 'siteSettingsSensors',
      icon: 'privacy:sensors',
      enabledLabel: 'siteSettingsMotionSensorsAllowed',
      disabledLabel: 'siteSettingsMotionSensorsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_SERIAL_PORTS,
      id: Id.SERIAL_PORTS,
      label: 'siteSettingsSerialPorts',
      icon: 'privacy:developer-board',
      enabledLabel: 'siteSettingsSerialPortsAllowed',
      disabledLabel: 'siteSettingsSerialPortsBlocked',
    },
    {
      route: routes.SITE_SETTINGS_SITE_DATA,
      id: Id.SITE_DATA,
      label: 'siteDataPageTitle',
      icon: 'privacy:database',
    },
    {
      route: routes.SITE_SETTINGS_SMART_CARD_READERS,
      id: Id.SMART_CARD_READERS,
      label: 'siteSettingsSmartCardReaders',
      icon: 'privacy:smart-card-reader',
      enabledLabel: 'siteSettingsSmartCardReadersAllowed',
      disabledLabel: 'siteSettingsSmartCardReadersBlocked',
      shouldShow: () =>
          loadTimeData.getBoolean('enableSmartCardReadersContentSetting'),
    },
    {
      route: routes.SITE_SETTINGS_SOUND,
      id: Id.SOUND,
      label: 'siteSettingsSound',
      icon: 'privacy:volume-up',
      enabledLabel: 'siteSettingsSoundAllowed',
      disabledLabel: 'siteSettingsSoundBlocked',
    },
    {
      route: routes.SITE_SETTINGS_STORAGE_ACCESS,
      id: Id.STORAGE_ACCESS,
      label: 'siteSettingsStorageAccess',
      icon: 'privacy:storage-access',
      enabledLabel: 'storageAccessAllowed',
      disabledLabel: 'storageAccessBlocked',
    },
    {
      route: routes.SITE_SETTINGS_USB_DEVICES,
      id: Id.USB_DEVICES,
      label: 'siteSettingsUsbDevices',
      icon: 'privacy:usb',
      enabledLabel: 'siteSettingsUsbAllowed',
      disabledLabel: 'siteSettingsUsbBlocked',
    },
    {
      route: routes.SITE_SETTINGS_VR,
      id: Id.VR,
      label: 'siteSettingsVr',
      icon: 'privacy:cardboard',
      enabledLabel: 'siteSettingsVrAllowed',
      disabledLabel: 'siteSettingsVrBlocked',
    },
    {
      route: routes.SITE_SETTINGS_WEB_APP_INSTALLATION,
      id: Id.WEB_APP_INSTALLATION,
      label: 'siteSettingsWebAppInstallation',
      icon: 'settings:install-desktop',
      enabledLabel: 'siteSettingsWebAppInstallationAsk',
      disabledLabel: 'siteSettingsWebAppInstallationBlock',
      shouldShow: () => loadTimeData.getBoolean('enableWebAppInstallation'),
    },
    {
      route: routes.SITE_SETTINGS_WEB_PRINTING,
      id: Id.WEB_PRINTING,
      label: 'siteSettingsWebPrinting',
      icon: 'settings:printer',
      enabledLabel: 'siteSettingsWebPrintingAsk',
      disabledLabel: 'siteSettingsWebPrintingBlock',
      shouldShow: () =>
          loadTimeData.getBoolean('enableWebPrintingContentSetting'),
    },
    {
      route: routes.SITE_SETTINGS_WINDOW_MANAGEMENT,
      id: Id.WINDOW_MANAGEMENT,
      label: 'siteSettingsWindowManagement',
      icon: 'privacy:select-window',
      enabledLabel: 'siteSettingsWindowManagementAsk',
      disabledLabel: 'siteSettingsWindowManagementBlocked',
    },
    {
      route: routes.SITE_SETTINGS_ZOOM_LEVELS,
      id: Id.ZOOM_LEVELS,
      label: 'siteSettingsZoomLevels',
      icon: 'privacy:zoom-in',
    },
  ];
  if (loadTimeData.getBoolean('is3pcdCookieSettingsRedesignEnabled') &&
      loadTimeData.getBoolean('isTrackingProtectionUxEnabled')) {
    categoryList.push({
      route: routes.TRACKING_PROTECTION,
      id: Id.COOKIES,
      label: 'trackingProtectionLinkRowLabel',
      icon: 'settings:visibility-off',
    });
  } else {
    categoryList.push({
      route: routes.COOKIES,
      id: Id.COOKIES,
      label: 'thirdPartyCookiesLinkRowLabel',
      icon: 'privacy:cookie',
    });
  }
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

const SettingsSiteSettingsPageElementBase =
    RouteObserverMixin(WebUiListenerMixin(PolymerElement));

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
          return {
            permissionsBasic: buildItemListFromIds([
              Id.GEOLOCATION,
              Id.CAMERA,
              Id.MIC,
              Id.NOTIFICATIONS,
              Id.STORAGE_ACCESS,
            ]),
            permissionsAdvanced: buildItemListFromIds([
              Id.BACKGROUND_SYNC,
              Id.SENSORS,
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
              Id.HAND_TRACKING,
              Id.IDLE_DETECTION,
              Id.WEB_PRINTING,
              Id.WINDOW_MANAGEMENT,
              Id.LOCAL_FONTS,
              Id.AUTO_PICTURE_IN_PICTURE,
              Id.CAPTURED_SURFACE_CONTROL,
              Id.KEYBOARD_LOCK,
              Id.POINTER_LOCK,
              Id.SMART_CARD_READERS,
              Id.WEB_APP_INSTALLATION,
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
              Id.PERFORMANCE,
              Id.JAVASCRIPT_OPTIMIZER,
              Id.AUTOMATIC_FULLSCREEN,
              Id.OFFER_WRITING_HELP,
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

      safetyHubAbusiveNotificationRevocationEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean(
            'safetyHubAbusiveNotificationRevocationEnabled'),
      },

      enableSafetyHub_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableSafetyHub');
        },
      },

      unusedSitePermissionsHeader_: String,
      unusedSitePermissionsSubeader_: String,
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
  private safetyHubAbusiveNotificationRevocationEnabled_: boolean;
  private unusedSitePermissionsHeader_: string;
  private unusedSitePermissionsSubheader_: string;
  private safetyHubBrowserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  private lists_: {
    all: CategoryListItem[],
    permissionsBasic: CategoryListItem[],
    permissionsAdvanced: CategoryListItem[],
    contentBasic: CategoryListItem[],
    contentAdvanced: CategoryListItem[],
  };

  override currentRouteChanged() {
    if (Router.getInstance().getCurrentRoute() !== routes.SITE_SETTINGS) {
      return;
    }
    // Only record the metrics when the user navigates to the privacy page
    // that shows the entry point.
    if (this.showUnusedSitePermissions_) {
      this.metricsBrowserProxy_.recordSafetyHubEntryPointShown(
          SafetyHubEntryPoint.SITE_SETTINGS);
    }
  }

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

  private async onUnusedSitePermissionListChanged_(
      permissions: UnusedSitePermissions[]) {
    // The unused site permissions review is shown when there are items to
    // review (provided the feature is enabled). Once visible it remains that
    // way to show completion info, even if the list is emptied.
    if (this.showUnusedSitePermissions_) {
      return;
    }

    this.showUnusedSitePermissions_ =
        (this.unusedSitePermissionsEnabled_ ||
         this.safetyHubAbusiveNotificationRevocationEnabled_) &&
        permissions.length > 0 && !loadTimeData.getBoolean('isGuest');
    this.unusedSitePermissionsHeader_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckUnusedSitePermissionsPrimaryLabel', permissions.length);
    // TODO(crbug/342210522): Add test for this.
    this.unusedSitePermissionsSubheader_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            this.safetyHubAbusiveNotificationRevocationEnabled_ ?
                'safetyHubRevokedPermissionsSecondaryLabel' :
                'safetyCheckUnusedSitePermissionsSecondaryLabel',
            permissions.length);
  }

  /** @return Class for the all site settings link */
  private getClassForSiteSettingsAllLink_(): string {
    return this.noRecentSitePermissions_ ? '' : 'hr';
  }

  private onSafetyHubButtonClick_() {
    this.metricsBrowserProxy_.recordSafetyHubEntryPointClicked(
        SafetyHubEntryPoint.SITE_SETTINGS);
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-site-settings-page': SettingsSiteSettingsPageElement;
  }
}

customElements.define(
    SettingsSiteSettingsPageElement.is, SettingsSiteSettingsPageElement);
