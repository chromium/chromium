// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, routes} from '../router.js';

import {OsSyncBrowserProxy, OsSyncBrowserProxyImpl, OsSyncPrefs} from './os_sync_browser_proxy.js';
import {getTemplate} from './os_sync_controls_subpage.html.js';

/**
 * Names of the radio buttons which allow the user to choose their data sync
 * mechanism.
 */
export enum RadioButtonNames {
  SYNC_EVERYTHING = 'sync-everything',
  CUSTOMIZE_SYNC = 'customize-sync',
}

/**
 * Names of the individual data type properties to be cached from
 * OsSyncPrefs when the user checks 'Sync All'.
 */
const SyncPrefsIndividualDataTypes: Array<keyof OsSyncPrefs> = [
  'osAppsSynced',
  'osPreferencesSynced',
  'osWifiConfigurationsSynced',

  // Note: Wallpaper uses a different naming scheme because it's stored as its
  // own separate pref instead of through the sync service.
  'wallpaperEnabled',
];

/**
 * TODO(https://crbug.com/1294178): Consider merging this with sync_controls.
 * @fileoverview
 * 'os-sync-controls-subpage' contains all OS sync data type controls.
 */
const OsSyncControlsSubpageElementBase = DeepLinkingMixin(
    WebUiListenerMixin(RouteObserverMixin(I18nMixin(PolymerElement))));

export class OsSyncControlsSubpageElement extends
    OsSyncControlsSubpageElementBase {
  static get is() {
    return 'os-sync-controls-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      hidden: {
        type: Boolean,
        value: true,
        computed: 'syncControlsHidden_(osSyncPrefs)',
        reflectToAttribute: true,
      },

      /**
       * The current OS sync preferences. Cached so we can restore individual
       * toggle state when turning "sync everything" on and off, without
       * affecting the underlying chrome prefs.
       */
      osSyncPrefs: Object,

      areDataTypeTogglesDisabled_: {
        type: Boolean,
        value: true,
        computed: `computeDataTypeTogglesDisabled_(osSyncPrefs.syncAllOsTypes)`,
      },

      /**
       * Whether to show the new UI for OS Sync Settings and
       * Browser Sync Settings  which include sublabel and
       * Apps toggle shared between Ash and Lacros.
       */
      showSyncSettingsRevamp_: {
        type: Boolean,
        value: loadTimeData.getBoolean('appsToggleSharingEnabled'),
        readOnly: true,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([Setting.kSplitSyncOnOff]),
      },
    };
  }

  private areDataTypeTogglesDisabled_: boolean;
  private showSyncSettingsRevamp_: boolean;
  private supportedSettingsIds: Set<Setting>;
  private browserProxy_: OsSyncBrowserProxy;
  private osSyncPrefs: OsSyncPrefs|undefined;
  private cachedOsSyncPrefs_: Partial<Record<keyof OsSyncPrefs, any>>|null;

  constructor() {
    super();
    this.browserProxy_ = OsSyncBrowserProxyImpl.getInstance();

    /**
     * Caches the individually selected synced data types. This is used to
     * be able to restore the selections after checking and unchecking Sync All.
     */
    this.cachedOsSyncPrefs_ = null;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'os-sync-prefs-changed', this.handleOsSyncPrefsChanged_.bind(this));
  }

  /**
   * RouteObserverMixin override
   */
  override currentRouteChanged(newRoute: Route, oldRoute: Route): void {
    if (newRoute === routes.OS_SYNC) {
      this.browserProxy_.didNavigateToOsSyncPage();
      this.attemptDeepLink();
    }
    if (oldRoute === routes.OS_SYNC) {
      this.browserProxy_.didNavigateAwayFromOsSyncPage();
    }
  }

  /**
   * Handler for when the sync preferences are updated.
   */
  private handleOsSyncPrefsChanged_(osSyncPrefs: OsSyncPrefs): void {
    this.osSyncPrefs = osSyncPrefs;

    // If apps are not registered or synced, force wallpaper off.
    if (!this.osSyncPrefs.osPreferencesSynced) {
      this.set('osSyncPrefs.wallpaperEnabled', false);
    }
  }

  /**
   * Computed binding returning the selected sync data radio button.
   */
  private selectedSyncDataRadio_(): RadioButtonNames {
    assertExists(this.osSyncPrefs);
    return this.osSyncPrefs.syncAllOsTypes ? RadioButtonNames.SYNC_EVERYTHING :
                                             RadioButtonNames.CUSTOMIZE_SYNC;
  }

  /**
   * Called when the sync data radio button selection changes.
   */
  private onSyncDataRadioSelectionChanged_(event: CustomEvent<{value: string}>):
      void {
    assertExists(this.osSyncPrefs);
    const syncAllDataTypes =
        event.detail.value === RadioButtonNames.SYNC_EVERYTHING;
    this.set('osSyncPrefs.syncAllOsTypes', syncAllDataTypes);
    if (syncAllDataTypes) {
      // Cache the previously selected preference before checking every box.
      this.cachedOsSyncPrefs_ = {};
      for (const dataType of SyncPrefsIndividualDataTypes) {
        // These are all booleans, so this shallow copy is sufficient.
        this.cachedOsSyncPrefs_[dataType] = this.osSyncPrefs[dataType];
        this.set(['osSyncPrefs', dataType], true);
      }
    } else if (this.cachedOsSyncPrefs_) {
      // Restore the previously selected preference.
      for (const dataType of SyncPrefsIndividualDataTypes) {
        this.set(['osSyncPrefs', dataType], this.cachedOsSyncPrefs_[dataType]);
      }
    }

    this.sendOsSyncDatatypes_();
  }

  /**
   * Called when the link to the browser's sync settings is clicked.
   */
  private onBrowserSyncSettingsClicked_(event: CustomEvent<{event: Event}>):
      void {
    // Prevent the default link click behavior.
    event.detail.event.preventDefault();

    // Programmatically open browser's sync settings.
    chrome.send('OpenBrowserSyncSettings');
  }

  /**
   * Handler for when any sync data type checkbox is changed.
   */
  private onSingleSyncDataTypeChanged_(): void {
    this.sendOsSyncDatatypes_();
  }

  /**
   * Handler for changes to the settings sync state; settings have a special
   * handler instead of relying on onSingleSyncDataTypeChanged_() because
   * wallpaper has a dependency on it.
   */
  private onSettingsSyncedChanged_(): void {
    this.set(
        'osSyncPrefs.wallpaperEnabled', this.osSyncPrefs!.osPreferencesSynced);

    this.onSingleSyncDataTypeChanged_();
  }

  /**
   * Sends the osSyncPrefs dictionary back to the C++ handler.
   */
  private sendOsSyncDatatypes_(): void {
    assertExists(this.osSyncPrefs);
    this.browserProxy_.setOsSyncDatatypes(this.osSyncPrefs);
  }

  /**
   * Whether the sync data type toggles should be disabled.
   */
  private computeDataTypeTogglesDisabled_(): boolean {
    return this.osSyncPrefs !== undefined && this.osSyncPrefs!.syncAllOsTypes;
  }

  /**
   * Whether the sync controls are hidden.
   */
  private syncControlsHidden_(): boolean {
    // Hide everything until the initial prefs are received from C++,
    // otherwise there is a visible layout reshuffle on first load.
    return !this.osSyncPrefs;
  }

  /**
   * Whether the wallpaper checkbox and label should be
   *     disabled.
   */
  private shouldWallpaperSyncSectionBeDisabled_(): boolean {
    return this.areDataTypeTogglesDisabled_ || !this.osSyncPrefs ||
        !this.osSyncPrefs.osPreferencesSynced;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSyncControlsSubpageElement.is]: OsSyncControlsSubpageElement;
  }
}

customElements.define(
    OsSyncControlsSubpageElement.is, OsSyncControlsSubpageElement);
