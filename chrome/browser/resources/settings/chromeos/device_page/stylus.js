// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-stylus' is the settings subpage with stylus-specific settings.
 */

const FIND_MORE_APPS_URL = 'https://play.google.com/store/apps/' +
    'collection/promotion_30023cb_stylus_apps';

Polymer({
  is: 'settings-stylus',

  behaviors: [
    DeepLinkingBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Policy indicator type for user policy - used for policy indicator UI
     * shown when an app that is not allowed to run on lock screen by policy is
     * selected.
     * @type {CrPolicyIndicatorType}
     * @private
     */
    userPolicyIndicator_: {
      type: String,
      value: CrPolicyIndicatorType.USER_POLICY,
    },

    /**
     * Note taking apps the user can pick between.
     * @private {Array<!settings.NoteAppInfo>}
     */
    appChoices_: {
      type: Array,
      value() {
        return [];
      }
    },

    /**
     * True if the device has an internal stylus.
     * @private
     */
    hasInternalStylus_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('hasInternalStylus');
      },
      readOnly: true,
    },

    /**
     * Currently selected note taking app.
     * @private {?settings.NoteAppInfo}
     */
    selectedApp_: {
      type: Object,
      value: null,
    },

    /**
     * True if the ARC container has not finished starting yet.
     * @private
     */
    waitingForAndroid_: {
      type: Boolean,
      value: false,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kStylusToolsInShelf,
        chromeos.settings.mojom.Setting.kStylusNoteTakingApp,
        chromeos.settings.mojom.Setting.kStylusNoteTakingFromLockScreen,
        chromeos.settings.mojom.Setting.kStylusLatestNoteOnLockScreen,
      ]),
    },
  },

  /**
   * @param {!settings.Route} route
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.STYLUS) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @return {boolean} Whether note taking from the lock screen is supported
   *     by the selected note-taking app.
   * @private
   */
  supportsLockScreen_() {
    return !!this.selectedApp_ &&
        this.selectedApp_.lockScreenSupport !==
        settings.NoteAppLockScreenSupport.NOT_SUPPORTED;
  },

  /**
   * @return {boolean} Whether the selected app is disallowed to handle note
   *     actions from lock screen as a result of a user policy.
   * @private
   */
  disallowedOnLockScreenByPolicy_() {
    return !!this.selectedApp_ &&
        this.selectedApp_.lockScreenSupport ===
        settings.NoteAppLockScreenSupport.NOT_ALLOWED_BY_POLICY;
  },

  /**
   * @return {boolean} Whether the selected app is enabled as a note action
   *     handler on the lock screen.
   * @private
   */
  lockScreenSupportEnabled_() {
    return !!this.selectedApp_ &&
        this.selectedApp_.lockScreenSupport ===
        settings.NoteAppLockScreenSupport.ENABLED;
  },

  /** @private {?settings.DevicePageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.DevicePageBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.browserProxy_.setNoteTakingAppsUpdatedCallback(
        this.onNoteAppsUpdated_.bind(this));
    this.browserProxy_.requestNoteTakingApps();
  },

  /**
   * Finds note app info with the provided app id.
   * @param {!string} id
   * @return {?settings.NoteAppInfo}
   * @private
   */
  findApp_(id) {
    return this.appChoices_.find(function(app) {
      return app.value === id;
    }) ||
        null;
  },

  /**
   * Toggles whether the selected app is enabled as a note action handler on
   * the lock screen.
   * @private
   */
  toggleLockScreenSupport_() {
    assert(this.selectedApp_);
    if (this.selectedApp_.lockScreenSupport !==
            settings.NoteAppLockScreenSupport.ENABLED &&
        this.selectedApp_.lockScreenSupport !==
            settings.NoteAppLockScreenSupport.SUPPORTED) {
      return;
    }

    this.browserProxy_.setPreferredNoteTakingAppEnabledOnLockScreen(
        this.selectedApp_.lockScreenSupport ===
        settings.NoteAppLockScreenSupport.SUPPORTED);
    settings.recordSettingChange();
  },

  /** @private */
  onSelectedAppChanged_() {
    const app = this.findApp_(this.$.selectApp.value);
    this.selectedApp_ = app;

    if (app && !app.preferred) {
      this.browserProxy_.setPreferredNoteTakingApp(app.value);
      settings.recordSettingChange();
    }
  },

  /**
   * @param {Array<!settings.NoteAppInfo>} apps
   * @param {boolean} waitingForAndroid
   * @private
   */
  onNoteAppsUpdated_(apps, waitingForAndroid) {
    this.waitingForAndroid_ = waitingForAndroid;
    this.appChoices_ = apps;

    // Wait until app selection UI is updated before setting the selected app.
    this.async(this.onSelectedAppChanged_.bind(this));
  },

  /**
   * @param {Array<!settings.NoteAppInfo>} apps
   * @param {boolean} waitingForAndroid
   * @private
   */
  showNoApps_(apps, waitingForAndroid) {
    return apps.length === 0 && !waitingForAndroid;
  },

  /**
   * @param {Array<!settings.NoteAppInfo>} apps
   * @param {boolean} waitingForAndroid
   * @private
   */
  showApps_(apps, waitingForAndroid) {
    return apps.length > 0 && !waitingForAndroid;
  },

  /** @private */
  onFindAppsTap_() {
    this.browserProxy_.showPlayStore(FIND_MORE_APPS_URL);
  },
});
