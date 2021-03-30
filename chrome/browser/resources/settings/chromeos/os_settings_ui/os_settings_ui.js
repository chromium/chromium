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
cr.define('settings', function() {
  /** Global defined when the main Settings script runs. */
  let defaultResourceLoaded = true;  // eslint-disable-line prefer-const

  assert(
      !window.settings || !settings.defaultResourceLoaded,
      'settings_ui.js run twice. You probably have an invalid import.');


  Polymer({
    is: 'os-settings-ui',

    behaviors: [
      CrContainerShadowBehavior,
      FindShortcutBehavior,
      // Calls currentRouteChanged() in attached(), so ensure other behaviors
      // run their attached() first.
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
       * Whether settings is in the narrow state (side nav hidden). Controlled
       * by a binding in the os-toolbar element.
       */
      isNarrow: {
        type: Boolean,
        value: false,
        readonly: true,
        notify: true,
        observer: 'onNarrowChanged_',
      },

      /**
       * @private {!OSPageVisibility}
       */
      pageVisibility_: {type: Object, value: settings.osPageVisibility},

      /** @private */
      havePlayStoreApp_: Boolean,

      /** @private */
      showAndroidApps_: Boolean,

      /** @private */
      showCrostini_: Boolean,

      /** @private */
      showToolbar_: Boolean,

      /** @private */
      showNavMenu_: Boolean,

      /** @private */
      showPluginVm_: Boolean,

      /** @private */
      showReset_: Boolean,

      /** @private */
      showStartup_: Boolean,

      /** @private */
      showKerberosSection_: Boolean,

      /** @private */
      lastSearchQuery_: {
        type: String,
        value: '',
      },

      /**
       * The threshold at which the toolbar will change from normal to narrow
       * mode, in px.
       * @private {boolean}
       */
      narrowThreshold_: {
        type: Number,
        value: 980,
      },
    },

    listeners: {
      'refresh-pref': 'onRefreshPref_',
      'user-action-setting-change': 'onSettingChange_',
    },

    /**
     * The route of the selected element in os-settings-menu. Stored here to
     * defer navigation until drawer animation completes.
     * @private {settings.Route}
     */
    activeRoute_: null,

    /**
     * Converts prefs to settings metrics to help record pref changes.
     * @private {PrefToSettingMetricConverter}
     */
    prefToSettingMetricConverter_: null,

    /** @override */
    created() {
      settings.Router.getInstance().initializeRouteFromUrl();
      this.prefToSettingMetricConverter_ = new PrefToSettingMetricConverter();
    },

    /**
     * @override
     * @suppress {es5Strict} Object literals cannot contain duplicate keys in
     * ES5 strict mode.
     */
    ready() {
      window.CrPolicyStrings = {
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

      this.havePlayStoreApp_ = loadTimeData.getBoolean('havePlayStoreApp');
      this.showAndroidApps_ = loadTimeData.getBoolean('androidAppsVisible');
      this.showCrostini_ = loadTimeData.getBoolean('showCrostini');
      this.showPluginVm_ = loadTimeData.getBoolean('showPluginVm');
      this.showNavMenu_ = !loadTimeData.getBoolean('isKioskModeActive');
      this.showToolbar_ = !loadTimeData.getBoolean('isKioskModeActive');
      this.showReset_ = loadTimeData.getBoolean('allowPowerwash');
      this.showStartup_ = loadTimeData.getBoolean('showStartup');

      this.showKerberosSection_ =
          loadTimeData.valueExists('isKerberosEnabled') &&
          loadTimeData.getBoolean('isKerberosEnabled') &&
          loadTimeData.valueExists('isKerberosSettingsSectionEnabled') &&
          loadTimeData.getBoolean('isKerberosSettingsSectionEnabled');

      this.addEventListener('show-container', () => {
        this.$.container.style.visibility = 'visible';
      });

      this.addEventListener('hide-container', () => {
        this.$.container.style.visibility = 'hidden';
      });

      // If navigation menu is not shown, do not listen to the drawer.
      if (!this.showNavMenu_) {
        return;
      }

      this.async(() => {
        // Lazy-create the drawer the first time it is opened or swiped into
        // view.
        const drawer = /** @type {!CrDrawerElement} */ (this.$$('#drawer'));
        assert(drawer);
        listenOnce(drawer, 'cr-drawer-opening', () => {
          this.$$('#drawerTemplate').if = true;
        });

        window.addEventListener('popstate', e => {
          drawer.cancel();
        });
      });
    },

    /** @override */
    attached() {
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

      // Window event listeners will not fire when settings first starts.
      // Blur events before the first focus event do not matter.
      if (document.hasFocus()) {
        settings.recordPageFocus();
      }

      window.addEventListener('focus', settings.recordPageFocus);
      window.addEventListener('blur', settings.recordPageBlur);

      // Clicks need to be captured because unlike focus/blur to the settings
      // window, a click's propagation can be stopped by child elements.
      window.addEventListener('click', settings.recordClick, /*capture=*/true);
    },

    /** @override */
    detached() {
      window.removeEventListener('focus', settings.recordPageFocus);
      window.removeEventListener('blur', settings.recordPageBlur);
      window.removeEventListener('click', settings.recordClick);
      settings.Router.getInstance().resetRouteForTesting();
    },

    /**
     * @param {!settings.Route} newRoute
     * @param {!settings.Route} oldRoute
     */
    currentRouteChanged(newRoute, oldRoute) {
      if (oldRoute && newRoute !== oldRoute) {
        // Search triggers route changes and currentRouteChanged() is called
        // in attached() state which is extraneous for this metric.
        settings.recordNavigation();
      }

      if (newRoute.depth <= 1) {
        // Main page uses scroll visibility to determine shadow.
        this.enableShadowBehavior(true);
      } else {
        // Sub-pages always show the top-container shadow.
        this.enableShadowBehavior(false);
        this.showDropShadows();
      }

      if (loadTimeData.getBoolean('newOsSettingsSearch')) {
        // TODO(crbug/1080777): Remove when new os settings search complete.
        // This block prevents the old settings search code from being executed.
        return;
      }

      const urlSearchQuery =
          settings.Router.getInstance().getQueryParameters().get('search') ||
          '';

      if (urlSearchQuery === this.lastSearchQuery_) {
        return;
      }

      this.lastSearchQuery_ = urlSearchQuery;

      // If toolbar is hidden, do not update anything.
      if (!this.showToolbar_) {
        return;
      }

      const toolbar = /** @type {!OsToolbarElement} */ (this.$$('os-toolbar'));
      const searchField =
          /** @type {?CrToolbarSearchFieldElement} */ (
              toolbar.getSearchField());

      if (!searchField) {
        // TODO(crbug/1080777): Remove this and surrounding code when new os
        // settings search complete. If the search field has not been rendered
        // yet, do not continue. crbug/1056909 changes the toolbar search field
        // to an optional value, so the element is not attached to the DOM the
        // first time this runs when the new OS Settings search flag is not
        // flipped on.
        return;
      }

      // If the search was initiated by directly entering a search URL, need to
      // sync the URL parameter to the textbox.
      if (urlSearchQuery !== searchField.getValue()) {
        // Setting the search box value without triggering a 'search-changed'
        // event, to prevent an unnecessary duplicate entry in |window.history|.
        searchField.setValue(urlSearchQuery, true /* noEvent */);
      }

      settings.recordSearch();
      /** @type {!OsSettingsMainElement} */ (
          this.$.main.searchContents(urlSearchQuery));
    },

    // Override FindShortcutBehavior methods.
    handleFindShortcut(modalContextOpen) {
      if (modalContextOpen || !this.showToolbar_) {
        return false;
      }
      this.$$('os-toolbar').getSearchField().showAndFocus();
      this.$$('os-toolbar').getSearchField().getSearchInput().select();
      return true;
    },

    // Override FindShortcutBehavior methods.
    searchInputHasFocus() {
      if (!this.showToolbar_) {
        return;
      }
      return this.$$('os-toolbar').getSearchField().isSearchFocused();
    },

    /**
     * @param {!CustomEvent<string>} e
     * @private
     */
    onRefreshPref_(e) {
      return /** @type {SettingsPrefsElement} */ (this.$.prefs)
          .refresh(e.detail);
    },

    /**
     * @param {!CustomEvent<!{prefKey: string, prefValue: *}>} e
     * @private
     */
    onSettingChange_(e) {
      const {prefKey, prefValue} = e.detail;
      const settingMetric =
          this.prefToSettingMetricConverter_.convertPrefToSettingMetric(
              prefKey, prefValue);

      // New metrics for this setting pref have not yet been implemented.
      if (!settingMetric) {
        settings.recordSettingChange();
        return;
      }

      const setting = /** @type {!chromeos.settings.mojom.Setting} */ (
          settingMetric.setting);
      const value = /** @type {!chromeos.settings.mojom.SettingChangeValue} */ (
          settingMetric.value);
      settings.recordSettingChange(setting, value);
    },

    /**
     * Handles the 'search-changed' event fired from the toolbar.
     * TODO(crbug/1080777): Remove when new settings search complete.
     * @param {!Event} e
     * @private
     */
    onSearchChanged_(e) {
      if (loadTimeData.getBoolean('newOsSettingsSearch')) {
        return;
      }
      const query = e.detail;
      settings.Router.getInstance().navigateTo(
          settings.routes.BASIC,
          query.length > 0 ?
              new URLSearchParams('search=' + encodeURIComponent(query)) :
              undefined,
          /* removeSearch */ true);
    },

    /**
     * Called when a section is selected.
     * @param {!Event} e
     * @private
     */
    onIronActivate_(e) {
      assert(this.showNavMenu_);
      const section = e.detail.selected;
      const path = new URL(section).pathname;
      const route = settings.Router.getInstance().getRouteForPath(path);
      assert(route, 'os-settings-menu has an entry with an invalid route.');
      this.activeRoute_ = route;

      if (this.isNarrow) {
        // If the onIronActivate event came from the drawer, close the drawer
        // and wait for the menu to close before navigating to |activeRoute_|.
        this.$$('#drawer').close();
        return;
      }
      this.navigateToActiveRoute_();
    },

    /** @private */
    onMenuButtonTap_() {
      if (!this.showNavMenu_) {
        return;
      }
      this.$$('#drawer').toggle();
    },

    /**
     * Navigates to |activeRoute_| if set. Used to delay navigation until after
     * animations complete to ensure focus ends up in the right place.
     * @private
     */
    navigateToActiveRoute_() {
      if (this.activeRoute_) {
        settings.Router.getInstance().navigateTo(
            this.activeRoute_, /* dynamicParams */ null,
            /* removeSearch */ true);
        this.activeRoute_ = null;
      }
    },

    /**
     * When this is called, The drawer animation is finished, and the dialog no
     * longer has focus. The selected section will gain focus if one was
     * selected. Otherwise, the drawer was closed due being canceled, and the
     * main settings container is given focus. That way the arrow keys can be
     * used to scroll the container, and pressing tab focuses a component in
     * settings.
     * @private
     */
    onMenuClose_() {
      if (!this.$$('#drawer').wasCanceled()) {
        // If a navigation happened, MainPageBehavior#currentRouteChanged
        // handles focusing the corresponding section when we call
        // settings.NavigateTo().
        this.navigateToActiveRoute_();
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
    onAdvancedOpenedInMainChanged_() {
      // Only sync value when opening, not closing.
      if (this.advancedOpenedInMain_) {
        this.advancedOpenedInMenu_ = true;
      }
    },

    /** @private */
    onAdvancedOpenedInMenuChanged_() {
      // Only sync value when opening, not closing.
      if (this.advancedOpenedInMenu_) {
        this.advancedOpenedInMain_ = true;
      }
    },

    /** @private */
    onNarrowChanged_() {
      if (this.showNavMenu_ && this.$$('#drawer').open && !this.isNarrow) {
        this.$$('#drawer').close();
      }
    },
  });

  // #cr_define_end
  return {defaultResourceLoaded};
});
