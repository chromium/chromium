// Copyright 2015 The Chromium Authors. All rights reserved.
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
  is: 'settings-ui',

  behaviors: [
    settings.RouteObserverBehavior,
    CrContainerShadowBehavior,
    FindShortcutBehavior,
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

    /** @private */
    narrow_: {
      type: Boolean,
      observer: 'onNarrowChanged_',
    },

    /**
     * @private {!PageVisibility}
     */
    pageVisibility_: {type: Object, value: settings.pageVisibility},

    /** @private */
    showAndroidApps_: Boolean,

    /** @private */
    showCrostini_: Boolean,

    /** @private */
    showParentalControls_: Boolean,

    /** @private */
    showPluginVm_: Boolean,

    /** @private */
    havePlayStoreApp_: Boolean,

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
      // <if expr="chromeos">
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
      // </if>
    };

    // <if expr="chromeos">
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
    // </if>

    // The SplitSettings feature hides OS settings in the browser settings page.
    // https://crbug.com/950007
    const showOSSettings = loadTimeData.getBoolean('showOSSettings');
    this.showAndroidApps_ = showOSSettings &&
        loadTimeData.valueExists('androidAppsVisible') &&
        loadTimeData.getBoolean('androidAppsVisible');
    this.showCrostini_ = showOSSettings &&
        loadTimeData.valueExists('showCrostini') &&
        loadTimeData.getBoolean('showCrostini');
    this.showParentalControls_ = showOSSettings &&
        loadTimeData.valueExists('showParentalControls') &&
        loadTimeData.getBoolean('showParentalControls');
    this.showPluginVm_ = showOSSettings &&
        loadTimeData.valueExists('showPluginVm') &&
        loadTimeData.getBoolean('showPluginVm');
    this.havePlayStoreApp_ = showOSSettings &&
        loadTimeData.valueExists('havePlayStoreApp') &&
        loadTimeData.getBoolean('havePlayStoreApp');

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

      // When transitioning  back to main page from a subpage on ChromeOS, using
      // 'smooth' scroll here results in the scroll changing to whatever is last
      // value of |top|. This happens even after setting the scroll position the
      // UI or programmatically.
      const behavior = cr.isChromeOS ? 'auto' : 'smooth';
      this.$.container.scrollTo({top: top, behavior: behavior});
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
    const urlSearchQuery = settings.getQueryParameters().get('search') || '';
    if (urlSearchQuery == this.lastSearchQuery_) {
      return;
    }

    this.lastSearchQuery_ = urlSearchQuery;

    const toolbar = /** @type {!CrToolbarElement} */ (this.$$('cr-toolbar'));
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
    this.$$('cr-toolbar').getSearchField().showAndFocus();
    return true;
  },

  // Override FindShortcutBehavior methods.
  searchInputHasFocus: function() {
    return this.$$('cr-toolbar').getSearchField().isSearchFocused();
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
    if (this.advancedOpenedInMain_) {
      this.advancedOpenedInMenu_ = true;
    }
  },

  /** @private */
  onAdvancedOpenedInMenuChanged_: function() {
    if (this.advancedOpenedInMenu_) {
      this.advancedOpenedInMain_ = true;
    }
  },

  /** @private */
  onNarrowChanged_: function() {
    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
    }
  },
});
