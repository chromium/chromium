// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * For each line in the item list, the text field will be shown in normal style
 * at front of the line. The highlightSuffix will be appended to the end of line
 * and emphasized with bold font.
 * @typedef {{
 *   text: string,
 *   highlightSuffix: ?string,
 * }}
 */
settings.ChromeCleanupRemovalListItem;

/**
 * The default number of items to show for files, registry keys and extensions
 * on the detailed view when user-initiated cleanups are enabled.
 */
settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW = 4;

/**
 * @fileoverview
 * 'items-to-remove-list' represents a list of items to
 * be removed or changed to be shown on the Chrome Cleanup page.
 * TODO(crbug.com/776538): Update the strings to say that some items are only
 *                         changed and not removed.
 *
 * Example:
 *
 *    <!-- Items list initially shows |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW|
 *         items. If there are more than |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW|
 *         items on the list, then a "show more" link is shown; tapping on it
 *         expands the list. -->
 *    <items-to-remove-list
 *        title="Files and programs:"
 *        items-to-show="[[filesToShow]]">
 *    </items-to-remove-list>
 */
Polymer({
  is: 'items-to-remove-list',

  properties: {
    title: {
      type: String,
      value: '',
    },

    /** @type {!Array<settings.ChromeCleanupRemovalListItem>} */
    itemsToShow: {
      type: Array,
      observer: 'updateVisibleState_',
    },

    /**
     * If true, all items from |itemsToShow| will be presented on the card,
     * and the "show more" link will be omitted.
     */
    expanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * The first |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW| items of |itemsToShow|
     * if the list is longer than |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW|.
     * @private {?Array<settings.ChromeCleanupRemovalListItem>}
     */
    initialItems_: Array,

    /**
     * The remaining items to be presented that are not included in
     * |initialItems_|. Items in this list are only shown to the user if
     * |expanded_| is true.
     * @private {?Array<settings.ChromeCleanupRemovalListItem>}
     */
    remainingItems_: Array,

    /**
     * The text for the "show more" link available if not all files are visible
     * in the card.
     * @private
     */
    moreItemsLinkText_: {
      type: String,
      value: '',
    },
  },

  /** @private */
  expandList_: function() {
    this.expanded_ = true;
    this.moreItemsLinkText_ = '';
  },

  /**
   * Decides which elements will be visible in the card and if the "show more"
   * link will be rendered.
   *
   * 1. If size(itemsToShow) < CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW, then all
   *    items will be visible.
   * 2. Otherwise, exactly |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW - 1| will be
   *    visible and the "show more" link will be rendered. The list presented to
   *    the user will contain exactly |CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW|
   *    elements, and the last one will be the "show more" link.
   *
   * @param {!Array<settings.ChromeCleanupRemovalListItem>} itemsToShow
   */
  updateVisibleState_: function(itemsToShow) {
    // Start expanded if there are less than
    // |settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW| items to show.
    this.expanded_ =
        itemsToShow.length <= settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW;

    if (this.expanded_) {
      this.initialItems_ = itemsToShow;
      this.remainingItems_ = [];
      this.moreItemsLinkText_ = '';
      return;
    }

    this.initialItems_ =
        itemsToShow.slice(0, settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW - 1);
    this.remainingItems_ =
        itemsToShow.slice(settings.CHROME_CLEANUP_DEFAULT_ITEMS_TO_SHOW - 1);

    const browserProxy = settings.ChromeCleanupProxyImpl.getInstance();
    browserProxy.getMoreItemsPluralString(this.remainingItems_.length)
        .then(linkText => {
          this.moreItemsLinkText_ = linkText;
        });
  },

  /**
   * Returns the class for the <li> elements that correspond to the items hidden
   * in the default view.
   * @param {boolean} expanded
   */
  remainingItemsClass_: function(expanded) {
    return expanded ? 'visible-item' : 'hidden-item';
  },

  /**
   * @param {settings.ChromeCleanupRemovalListItem} item
   * @return {boolean} Whether a highlight suffix exists.
   * @private
   */
  hasHighlightSuffix_: function(item) {
    return item.highlightSuffix !== null;
  },
});
