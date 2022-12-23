// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared.css.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {Route} from '../router.js';

import {OsSyncBrowserProxy, OsSyncBrowserProxyImpl, OsSyncPrefs} from './os_sync_browser_proxy.js';
import {getTemplate} from './os_sync_controls.html.js';

/**
 * Names of the radio buttons which allow the user to choose their data sync
 * mechanism.
 * @enum {string}
 */
const RadioButtonNames = {
  SYNC_EVERYTHING: 'sync-everything',
  CUSTOMIZE_SYNC: 'customize-sync',
};

/**
 * Names of the individual data type properties to be cached from
 * OsSyncPrefs when the user checks 'Sync All'.
 * @type {!Array<string>}
 */
const SyncPrefsIndividualDataTypes = [
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
 * 'os-sync-controls' contains all OS sync data type controls.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const OsSyncControlsElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteObserverBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/** @polymer */
class OsSyncControlsElement extends OsSyncControlsElementBase {
  static get is() {
    return 'os-sync-controls';
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
       * @type {OsSyncPrefs|undefined}
       */
      osSyncPrefs: Object,

      /** @private */
      areDataTypeTogglesDisabled_: {
        type: Boolean,
        value: true,
        computed: `computeDataTypeTogglesDisabled_(osSyncPrefs.syncAllOsTypes)`,
      },

      /**
       * Whether to show apps checkbox sublabel, which contains disclaimer about
       * Web apps and relevant if checkbox value is shared with Lacros.
       */
      showAppsCheckboxSublabel_: {
        type: Boolean,
        value: loadTimeData.getBoolean('appsToggleSharingEnabled'),
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kSplitSyncOnOff]),
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!OsSyncBrowserProxy} */
    this.browserProxy_ = OsSyncBrowserProxyImpl.getInstance();

    /**
     * Caches the individually selected synced data types. This is used to
     * be able to restore the selections after checking and unchecking Sync All.
     * @private {?Object}
     */
    this.cachedOsSyncPrefs_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'os-sync-prefs-changed', this.handleOsSyncPrefsChanged_.bind(this));
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
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
   * @private
   */
  handleOsSyncPrefsChanged_(osSyncPrefs) {
    this.osSyncPrefs = osSyncPrefs;

    // If apps are not registered or synced, force wallpaper off.
    if (!this.osSyncPrefs.osPreferencesSynced) {
      this.set('osSyncPrefs.wallpaperEnabled', false);
    }
  }

  /**
   * Computed binding returning the selected sync data radio button.
   * @private
   */
  selectedSyncDataRadio_() {
    return this.osSyncPrefs.syncAllOsTypes ? RadioButtonNames.SYNC_EVERYTHING :
                                             RadioButtonNames.CUSTOMIZE_SYNC;
  }

  /**
   * Called when the sync data radio button selection changes.
   * @private
   */
  onSyncDataRadioSelectionChanged_(event) {
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
   * Handler for when any sync data type checkbox is changed.
   * @private
   */
  onSingleSyncDataTypeChanged_() {
    this.sendOsSyncDatatypes_();
  }

  /**
   * Handler for changes to the settings sync state; settings have a special
   * handler instead of relying on onSingleSyncDataTypeChanged_() because
   * wallpaper has a dependency on it.
   * @private
   */
  onSettingsSyncedChanged_() {
    this.set(
        'osSyncPrefs.wallpaperEnabled', this.osSyncPrefs.osPreferencesSynced);

    this.onSingleSyncDataTypeChanged_();
  }

  /**
   * Sends the osSyncPrefs dictionary back to the C++ handler.
   * @private
   */
  sendOsSyncDatatypes_() {
    assert(this.osSyncPrefs);
    this.browserProxy_.setOsSyncDatatypes(this.osSyncPrefs);
  }

  /**
   * @return {boolean} Whether the sync data type toggles should be disabled.
   * @private
   */
  computeDataTypeTogglesDisabled_() {
    return this.osSyncPrefs !== undefined && this.osSyncPrefs.syncAllOsTypes;
  }

  /**
   * @return {boolean} Whether the sync controls are hidden.
   * @private
   */
  syncControlsHidden_() {
    // Hide everything until the initial prefs are received from C++,
    // otherwise there is a visible layout reshuffle on first load.
    return !this.osSyncPrefs;
  }

  /**
   * @return {boolean} Whether the wallpaper checkbox and label should be
   *     disabled.
   * @private
   */
  shouldWallpaperSyncSectionBeDisabled_() {
    return this.areDataTypeTogglesDisabled_ || !this.osSyncPrefs ||
        !this.osSyncPrefs.osPreferencesSynced;
  }
}

customElements.define(OsSyncControlsElement.is, OsSyncControlsElement);
