// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ui' implements the UI for the Settings page.
 *
 * Example:
 *
 *    <settings-ui prefs="{{prefs}}"></settings-ui>
 */
cr.exportPath('settings');
assert(
    !settings.defaultResourceLoaded,
    'settings_ui.js run twice. You probably have an invalid import.');
/** Global defined when the main Settings script runs. */
settings.defaultResourceLoaded = true;

Polymer({
  is: 'os-settings-ui',

  behaviors: [
    CrContainerShadowBehavior,
    FindShortcutBehavior,
    // Calls currentRouteChanged() in attached(), so ensure other behaviors run
    // their attached() first.
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: Object,

    /** @private */
    advancedOpenedInMain_: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'onAdvancedOpenedInMainChanged_',
    },

    /** @private */
    advancedOpenedInMenu_: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'onAdvancedOpenedInMenuChanged_',
    },

    /** @private {boolean} */
    toolbarSpinnerActive_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether settings is in the narrow state (side nav hidden). Controlled by
     * a binding in the os-toolbar element.
     */
    isNarrow: {
      type: Boolean,
      observer: 'onNarrowChanged_',
    },

    /**
     * @private {!PageVisibility}
     */
    pageVisibility_: {type: Object, value: settings.pageVisibility},

    /** @private */
    havePlayStoreApp_: Boolean,

    /** @private */
    showAndroidApps_: Boolean,

    /** @private */
    showAppManagement_: Boolean,

    /** @private */
    showApps_: Boolean,

    /** @private */
    showCrostini_: Boolean,

    /** @private */
    showPluginVm_: Boolean,

    /** @private */
    showReset_: Boolean,

    /** @private */
    lastSearchQuery_: {
      type: String,
      value: '',
    },
  },

  listeners: {
    'refresh-pref': 'onRefreshPref_',
  },

  /** @override */
  created: function() {
    settings.initializeRouteFromUrl();
  },

  /**
   * @override
   * @suppress {es5Strict} Object literals cannot contain duplicate keys in ES5
   *     strict mode.
   */
  ready: function() {
    // Lazy-create the drawer the first time it is opened or swiped into view.
    listenOnce(this.$.drawer, 'cr-drawer-opening', () => {
      this.$.drawerTemplate.if = true;
    });

    window.addEventListener('popstate', e => {
      this.$.drawer.cancel();
    });

    CrPolicyStrings = {
      controlledSettingExtension:
          loadTimeData.getString('controlledSettingExtension'),
      controlledSettingExtensionWithoutName:
          loadTimeData.getString('controlledSettingExtensionWithoutName'),
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
      controlledSettingRecommendedMatches:
          loadTimeData.getString('controlledSettingRecommendedMatches'),
      controlledSettingRecommendedDiffers:
          loadTimeData.getString('controlledSettingRecommendedDiffers'),
      controlledSettingShared:
          loadTimeData.getString('controlledSettingShared'),
      controlledSettingWithOwner:
          loadTimeData.getString('controlledSettingWithOwner'),
      controlledSettingNoOwner:
          loadTimeData.getString('controlledSettingNoOwner'),
      controlledSettingParent:
          loadTimeData.getString('controlledSettingParent'),
      controlledSettingChildRestriction:
          loadTimeData.getString('controlledSettingChildRestriction'),
    };

    CrOncStrings = {
      OncTypeCellular: loadTimeData.getString('OncTypeCellular'),
      OncTypeEthernet: loadTimeData.getString('OncTypeEthernet'),
      OncTypeMobile: loadTimeData.getString('OncTypeMobile'),
      OncTypeTether: loadTimeData.getString('OncTypeTether'),
      OncTypeVPN: loadTimeData.getString('OncTypeVPN'),
      OncTypeWiFi: loadTimeData.getString('OncTypeWiFi'),
      networkListItemConnected:
          loadTimeData.getString('networkListItemConnected'),
      networkListItemConnecting:
          loadTimeData.getString('networkListItemConnecting'),
      networkListItemConnectingTo:
          loadTimeData.getString('networkListItemConnectingTo'),
      networkListItemInitializing:
          loadTimeData.getString('networkListItemInitializing'),
      networkListItemLabelTemplate:
          loadTimeData.getString('networkListItemLabelTemplate'),
      networkListItemNotAvailable:
          loadTimeData.getString('networkListItemNotAvailable'),
      networkListItemScanning:
          loadTimeData.getString('networkListItemScanning'),
      networkListItemSimCardLocked:
          loadTimeData.getString('networkListItemSimCardLocked'),
      networkListItemNotConnected:
          loadTimeData.getString('networkListItemNotConnected'),
      networkListItemNoNetwork:
          loadTimeData.getString('networkListItemNoNetwork'),
      vpnNameTemplate: loadTimeData.getString('vpnNameTemplate'),
    };

    this.havePlayStoreApp_ = loadTimeData.getBoolean('havePlayStoreApp');
    this.showAppManagement_ = loadTimeData.getBoolean('showAppManagement');
    this.showAndroidApps_ = loadTimeData.getBoolean('androidAppsVisible');
    this.showApps_ = this.showAppManagement_ || this.showAndroidApps_;
    this.showCrostini_ = loadTimeData.getBoolean('showCrostini');
    this.showPluginVm_ = loadTimeData.getBoolean('showPluginVm');
    this.showReset_ = loadTimeData.getBoolean('allowPowerwash');

    this.addEventListener('show-container', () => {
      this.$.container.style.visibility = 'visible';
    });

    this.addEventListener('hide-container', () => {
      this.$.container.style.visibility = 'hidden';
    });
  },

  /** @override */
  attached: function() {
    document.documentElement.classList.remove('loading');

    setTimeout(function() {
      chrome.send(
          'metricsHandler:recordTime',
          ['Settings.TimeUntilInteractive', window.performance.now()]);
    });

    // Preload bold Roboto so it doesn't load and flicker the first time used.
    document.fonts.load('bold 12px Roboto');
    settings.setGlobalScrollTarget(this.$.container);

    const scrollToTop = top => new Promise(resolve => {
      if (this.$.container.scrollTop === top) {
        resolve();
        return;
      }

      this.$.container.scrollTo({top: top, behavior: 'auto'});
      const onScroll = () => {
        this.debounce('scrollEnd', () => {
          this.$.container.removeEventListener('scroll', onScroll);
          resolve();
        }, 75);
      };
      this.$.container.addEventListener('scroll', onScroll);
    });
    this.addEventListener('scroll-to-top', e => {
      scrollToTop(e.detail.top).then(e.detail.callback);
    });
    this.addEventListener('scroll-to-bottom', e => {
      scrollToTop(e.detail.bottom - this.$.container.clientHeight)
          .then(e.detail.callback);
    });
  },

  /** @override */
  detached: function() {
    settings.resetRouteForTesting();
  },

  /** @param {!settings.Route} route */
  currentRouteChanged: function(route) {
    if (route.depth <= 1) {
      // Main page uses scroll visibility to determine shadow.
      this.enableShadowBehavior(true);
    } else {
      // Sub-pages always show the top-container shadow.
      this.enableShadowBehavior(false);
      this.showDropShadows();
    }

    const urlSearchQuery = settings.getQueryParameters().get('search') || '';
    if (urlSearchQuery == this.lastSearchQuery_) {
      return;
    }

    this.lastSearchQuery_ = urlSearchQuery;

    const toolbar = /** @type {!OsToolbarElement} */ (this.$$('os-toolbar'));
    const searchField =
        /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());

    // If the search was initiated by directly entering a search URL, need to
    // sync the URL parameter to the textbox.
    if (urlSearchQuery != searchField.getValue()) {
      // Setting the search box value without triggering a 'search-changed'
      // event, to prevent an unnecessary duplicate entry in |window.history|.
      searchField.setValue(urlSearchQuery, true /* noEvent */);
    }

    this.$.main.searchContents(urlSearchQuery);
  },

  // Override FindShortcutBehavior methods.
  handleFindShortcut: function(modalContextOpen) {
    if (modalContextOpen) {
      return false;
    }
    this.$$('os-toolbar').getSearchField().showAndFocus();
    return true;
  },

  // Override FindShortcutBehavior methods.
  searchInputHasFocus: function() {
    return this.$$('os-toolbar').getSearchField().isSearchFocused();
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onRefreshPref_: function(e) {
    return /** @type {SettingsPrefsElement} */ (this.$.prefs).refresh(e.detail);
  },

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   * @param {!Event} e
   * @private
   */
  onSearchChanged_: function(e) {
    const query = e.detail;
    settings.navigateTo(
        settings.routes.BASIC,
        query.length > 0 ?
            new URLSearchParams('search=' + encodeURIComponent(query)) :
            undefined,
        /* removeSearch */ true);
  },

  /**
   * Called when a section is selected.
   * @private
   */
  onIronActivate_: function() {
    this.$.drawer.close();
  },

  /** @private */
  onMenuButtonTap_: function() {
    this.$.drawer.toggle();
  },

  /**
   * When this is called, The drawer animation is finished, and the dialog no
   * longer has focus. The selected section will gain focus if one was selected.
   * Otherwise, the drawer was closed due being canceled, and the main settings
   * container is given focus. That way the arrow keys can be used to scroll
   * the container, and pressing tab focuses a component in settings.
   * @private
   */
  onMenuClose_: function() {
    if (!this.$.drawer.wasCanceled()) {
      // If a navigation happened, MainPageBehavior#currentRouteChanged handles
      // focusing the corresponding section.
      return;
    }

    // Add tab index so that the container can be focused.
    this.$.container.setAttribute('tabindex', '-1');
    this.$.container.focus();

    listenOnce(this.$.container, ['blur', 'pointerdown'], () => {
      this.$.container.removeAttribute('tabindex');
    });
  },

  /** @private */
  onAdvancedOpenedInMainChanged_: function() {
    // Only sync value when opening, not closing.
    if (this.advancedOpenedInMain_) {
      this.advancedOpenedInMenu_ = true;
    }
  },

  /** @private */
  onAdvancedOpenedInMenuChanged_: function() {
    // Only sync value when opening, not closing.
    if (this.advancedOpenedInMenu_) {
      this.advancedOpenedInMain_ = true;
    }
  },

  /** @private */
  onNarrowChanged_: function() {
    if (this.$.drawer.open && !this.isNarrow) {
      this.$.drawer.close();
    }
  },
});
