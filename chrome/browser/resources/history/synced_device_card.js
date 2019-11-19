// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-synced-device-card',

  properties: {
    /**
     * The list of tabs open for this device.
     * @type {!Array<!ForeignSessionTab>}
     */
    tabs: {
      type: Array,
      value: function() {
        return [];
      },
      observer: 'updateIcons_'
    },

    // Name of the synced device.
    device: String,

    // When the device information was last updated.
    lastUpdateTime: String,

    // Whether the card is open.
    opened: Boolean,

    searchTerm: String,

    /**
     * The indexes where a window separator should be shown. The use of a
     * separate array here is necessary for window separators to appear
     * correctly in search. See http://crrev.com/2022003002 for more details.
     * @type {!Array<number>}
     */
    separatorIndexes: Array,

    // Internal identifier for the device.
    sessionTag: String,
  },

  listeners: {
    'dom-change': 'notifyFocusUpdate_',
  },

  /**
   * Create FocusRows for this card. One is always made for the card heading and
   * one for each result if the card is open.
   * @return {!Array<!cr.ui.FocusRow>}
   */
  createFocusRows: function() {
    const titleRow = new cr.ui.FocusRow(this.$['card-heading'], null);
    titleRow.addItem('menu', '#menu-button');
    titleRow.addItem('collapse', '#collapse-button');
    const rows = [titleRow];
    if (this.opened) {
      this.shadowRoot.querySelectorAll('.item-container').forEach(function(el) {
        const row = new cr.ui.FocusRow(el, null);
        row.addItem('link', '.website-link');
        rows.push(row);
      });
    }
    return rows;
  },

  /**
   * Open a single synced tab.
   * @param {DomRepeatClickEvent} e
   * @private
   */
  openTab_: function(e) {
    const tab = /** @type {ForeignSessionTab} */ (e.model.tab);
    const browserService = history.BrowserService.getInstance();
    browserService.recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.LINK_CLICKED,
        SyncedTabsHistogram.LIMIT);
    browserService.openForeignSessionTab(
        this.sessionTag, tab.windowId, tab.sessionId, e);
    e.preventDefault();
  },

  /**
   * Toggles the dropdown display of synced tabs for each device card.
   */
  toggleTabCard: function() {
    const histogramValue = this.$.collapse.opened ?
        SyncedTabsHistogram.COLLAPSE_SESSION :
        SyncedTabsHistogram.EXPAND_SESSION;

    history.BrowserService.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, histogramValue, SyncedTabsHistogram.LIMIT);

    this.$.collapse.toggle();

    this.fire('update-focus-grid');
  },

  /** @private */
  notifyFocusUpdate_: function() {
    // Refresh focus after all rows are rendered.
    this.fire('update-focus-grid');
  },

  /**
   * When the synced tab information is set, the icon associated with the tab
   * website is also set.
   * @private
   */
  updateIcons_: function() {
    this.async(function() {
      const icons = this.shadowRoot.querySelectorAll('.website-icon');

      for (let i = 0; i < this.tabs.length; i++) {
        // Entries on this UI are coming strictly from sync, so we can set
        // |isSyncedUrlForHistoryUi| to true on the getFavicon call below.
        icons[i].style.backgroundImage = cr.icon.getFaviconForPageURL(
            this.tabs[i].url, true, this.tabs[i].remoteIconUrlForUma);
      }
    });
  },

  /** @private */
  isWindowSeparatorIndex_: function(index, separatorIndexes) {
    return this.separatorIndexes.indexOf(index) != -1;
  },

  /**
   * @param {boolean} opened
   * @return {string}
   * @private
   */
  getCollapseIcon_: function(opened) {
    return opened ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * @param {boolean} opened
   * @return {string}
   * @private
   */
  getCollapseTitle_: function(opened) {
    return opened ? loadTimeData.getString('collapseSessionButton') :
                    loadTimeData.getString('expandSessionButton');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onMenuButtonTap_: function(e) {
    this.fire('open-menu', {
      target: Polymer.dom(e).localTarget,
      tag: this.sessionTag,
    });
    e.stopPropagation();  // Prevent iron-collapse.
  },

  onLinkRightClick_: function() {
    history.BrowserService.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.LINK_RIGHT_CLICKED,
        SyncedTabsHistogram.LIMIT);
  },
});
