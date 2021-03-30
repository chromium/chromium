// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-menu' shows a menu with a hardcoded set of pages and subpages.
 */
Polymer({
  is: 'os-settings-menu',

  behaviors: [settings.RouteObserverBehavior],

  properties: {
    advancedOpened: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Whether the user is in guest mode.
     * @private{boolean}
     */
    isGuestMode_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isGuest'),
      readOnly: true,
    },

    showCrostini: Boolean,

    showStartup: Boolean,

    showReset: Boolean,

    showKerberosSection: Boolean,

    /**
     * True if redesign of account management flows is enabled.
     * @private
     */
    isAccountManagementFlowsV2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled');
      },
      readOnly: true,
    },
  },

  /** @param {!settings.Route} newRoute */
  currentRouteChanged(newRoute) {
    const urlSearchQuery =
        settings.Router.getInstance().getQueryParameters().get('search');
    // If the route navigated to by a search result is in the advanced
    // section, the advanced menu will expand.
    if (urlSearchQuery && settings.routes.ADVANCED &&
        settings.routes.ADVANCED.contains(newRoute)) {
      this.advancedOpened = true;
    }

    // Focus the initially selected path.
    const anchors = this.root.querySelectorAll('a');
    for (let i = 0; i < anchors.length; ++i) {
      const anchorRoute = settings.Router.getInstance().getRouteForPath(
          anchors[i].getAttribute('href'));
      if (anchorRoute && anchorRoute.contains(newRoute)) {
        this.setSelectedUrl_(anchors[i].href);
        return;
      }
    }

    this.setSelectedUrl_('');  // Nothing is selected.
  },

  /** @private */
  onAdvancedButtonToggle_() {
    this.advancedOpened = !this.advancedOpened;
  },

  /**
   * Prevent clicks on sidebar items from navigating. These are only links for
   * accessibility purposes, taps are handled separately by <iron-selector>.
   * @param {!Event} event
   * @private
   */
  onLinkClick_(event) {
    if (event.target.matches('a')) {
      event.preventDefault();
    }
  },

  /**
   * Keeps both menus in sync. |url| needs to come from |element.href| because
   * |iron-list| uses the entire url. Using |getAttribute| will not work.
   * @param {string} url
   */
  setSelectedUrl_(url) {
    this.$.topMenu.selected = this.$.subMenu.selected = url;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelectorActivate_(event) {
    this.setSelectedUrl_(event.detail.selected);
  },

  /**
   * @param {boolean} opened Whether the menu is expanded.
   * @return {string} Which icon to use.
   * @private
   * */
  arrowState_(opened) {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  },

  /** @return {boolean} Whether the advanced submenu is open. */
  isAdvancedSubmenuOpenedForTest() {
    const submenu = /** @type {IronCollapseElement} */ (this.$.advancedSubmenu);
    return submenu.opened;
  },

  /**
   * @param {boolean} bool
   * @return {string}
   * @private
   */
  boolToString_(bool) {
    return bool.toString();
  },
});
