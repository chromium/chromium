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
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_page_host_style_css.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import '../icons.js';
import '../settings_main/settings_main.js';
import '../settings_menu/settings_menu.js';
import '../settings_shared_css.js';
import '../prefs/prefs.js';
import '../settings_vars_css.js';

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {FindShortcutBehavior} from 'chrome://resources/cr_elements/find_shortcut_behavior.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {resetGlobalScrollTargetForTesting, setGlobalScrollTarget} from '../global_scroll_target_behavior.js';
import {loadTimeData} from '../i18n_setup.js';
import {PageVisibility, pageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.js';

Polymer({
  is: 'settings-ui',

  _template: html`{__html_template__}`,

  behaviors: [
    RouteObserverBehavior,
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
    pageVisibility_: {type: Object, value: pageVisibility},

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
  created() {
    Router.getInstance().initializeRouteFromUrl();
  },

  /**
   * @override
   * @suppress {es5Strict} Object literals cannot contain duplicate keys in
   * ES5 strict mode.
   */
  ready() {
    // Lazy-create the drawer the first time it is opened or swiped into view.
    listenOnce(this.$.drawer, 'cr-drawer-opening', () => {
      this.$.drawerTemplate.if = true;
    });

    window.addEventListener('popstate', e => {
      this.$.drawer.cancel();
    });

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

    this.addEventListener('show-container', () => {
      this.$.container.style.visibility = 'visible';
    });

    this.addEventListener('hide-container', () => {
      this.$.container.style.visibility = 'hidden';
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
    setGlobalScrollTarget(
        /** @type {HTMLElement} */ (this.$.container));

    const scrollToTop = top => new Promise(resolve => {
      if (this.$.container.scrollTop === top) {
        resolve();
        return;
      }

      // When transitioning  back to main page from a subpage on ChromeOS,
      // using 'smooth' scroll here results in the scroll changing to whatever
      // is last value of |top|. This happens even after setting the scroll
      // position the UI or programmatically.
      const behavior = isChromeOS ? 'auto' : 'smooth';
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
  detached() {
    Router.getInstance().resetRouteForTesting();
    resetGlobalScrollTargetForTesting();
  },

  /** @param {!Route} route */
  currentRouteChanged(route) {
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('search') || '';
    if (urlSearchQuery === this.lastSearchQuery_) {
      return;
    }

    this.lastSearchQuery_ = urlSearchQuery;

    const toolbar = /** @type {!CrToolbarElement} */ (this.$$('cr-toolbar'));
    const searchField =
        /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());

    // If the search was initiated by directly entering a search URL, need to
    // sync the URL parameter to the textbox.
    if (urlSearchQuery !== searchField.getValue()) {
      // Setting the search box value without triggering a 'search-changed'
      // event, to prevent an unnecessary duplicate entry in |window.history|.
      searchField.setValue(urlSearchQuery, true /* noEvent */);
    }

    this.$.main.searchContents(urlSearchQuery);
  },

  // Override FindShortcutBehavior methods.
  handleFindShortcut(modalContextOpen) {
    if (modalContextOpen) {
      return false;
    }
    this.$$('cr-toolbar').getSearchField().showAndFocus();
    return true;
  },

  // Override FindShortcutBehavior methods.
  searchInputHasFocus() {
    return this.$$('cr-toolbar').getSearchField().isSearchFocused();
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onRefreshPref_(e) {
    return /** @type {SettingsPrefsElement} */ (this.$.prefs).refresh(e.detail);
  },

  /**
   * Handles the 'search-changed' event fired from the toolbar.
   * @param {!Event} e
   * @private
   */
  onSearchChanged_(e) {
    const query = e.detail;
    Router.getInstance().navigateTo(
        routes.BASIC,
        query.length > 0 ?
            new URLSearchParams('search=' + encodeURIComponent(query)) :
            undefined,
        /* removeSearch */ true);
  },

  /**
   * Called when a section is selected.
   * @private
   */
  onIronActivate_() {
    this.$.drawer.close();
  },

  /** @private */
  onMenuButtonTap_() {
    this.$.drawer.toggle();
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
    if (!this.$.drawer.wasCanceled()) {
      // If a navigation happened, MainPageBehavior#currentRouteChanged
      // handles focusing the corresponding section.
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
    if (this.advancedOpenedInMain_) {
      this.advancedOpenedInMenu_ = true;
    }
  },

  /** @private */
  onAdvancedOpenedInMenuChanged_() {
    if (this.advancedOpenedInMenu_) {
      this.advancedOpenedInMain_ = true;
    }
  },

  /** @private */
  onNarrowChanged_() {
    if (this.$.drawer.open && !this.narrow_) {
      this.$.drawer.close();
    }

    const focusedElement = this.shadowRoot.activeElement;
    if (this.narrow_ && focusedElement === this.$.leftMenu) {
      // If changed from non-narrow to narrow and the focus was on the left
      // menu, move focus to the button that opens the drawer menu.
      this.$.toolbar.focusMenuButton();
    } else if (!this.narrow_ && this.$.toolbar.isMenuFocused()) {
      // If changed from narrow to non-narrow and the focus was on the button
      // that opens the drawer menu, move focus to the left menu.
      this.$.leftMenu.focusFirstItem();
    } else if (!this.narrow_ && focusedElement === this.$$('#drawerMenu')) {
      // If changed from narrow to non-narrow and the focus was in the drawer
      // menu, wait for the drawer to close and then move focus on the left
      // menu. The drawer has a dialog element in it so moving focus to an
      // element outside the dialog while it is open will not work.
      const boundCloseListener = () => {
        this.$.leftMenu.focusFirstItem();
        this.$.drawer.removeEventListener('close', boundCloseListener);
      };
      this.$.drawer.addEventListener('close', boundCloseListener);
    }
  },

  /**
   * Only used in tests.
   * @return {boolean}
   */
  getAdvancedOpenedInMainForTest() {
    return this.advancedOpenedInMain_;
  },

  /**
   * Only used in tests.
   * @return {boolean}
   */
  getAdvancedOpenedInMenuForTest() {
    return this.advancedOpenedInMenu_;
  }
});
