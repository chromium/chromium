// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searched_label.js';
import './shared_style.js';
import './strings.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/icon.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserService} from './browser_service.js';
import {UMA_MAX_BUCKET_VALUE, UMA_MAX_SUBSET_BUCKET_VALUE} from './constants.js';

Polymer({
  is: 'history-item',

  _template: html`{__html_template__}`,

  behaviors: [FocusRowBehavior],

  properties: {
    // Underlying HistoryEntry data for this item. Contains read-only fields
    // from the history backend, as well as fields computed by history-list.
    item: {
      type: Object,
      observer: 'itemChanged_',
    },

    selected: {
      type: Boolean,
      reflectToAttribute: true,
    },

    isCardStart: {
      type: Boolean,
      reflectToAttribute: true,
    },

    isCardEnd: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @type {Element} */
    lastFocused: {
      type: Object,
      notify: true,
    },

    listBlurred: {
      type: Boolean,
      notify: true,
    },

    ironListTabIndex: {
      type: Number,
      observer: 'ironListTabIndexChanged_',
    },

    selectionNotAllowed_: {
      type: Boolean,
      value: !loadTimeData.getBoolean('allowDeletingHistory'),
    },

    hasTimeGap: Boolean,

    index: Number,

    numberOfItems: Number,

    // Search term used to obtain this history-item.
    searchTerm: String,

    overrideCustomEquivalent: {
      type: Boolean,
      value: true,
    },

    /** @private */
    ariaDescribedByForHeading_: {
      type: String,
      computed: 'getAriaDescribedByForHeading_(isCardStart, isCardEnd)',
    },
  },

  hostAttributes: {'role': 'row'},

  /** @private {boolean} */
  mouseDown_: false,

  /** @private {boolean} */
  isShiftKeyDown_: false,

  /** @override */
  attached() {
    afterNextRender(this, function() {
      // Adding listeners asynchronously to reduce blocking time, since these
      // history items are items in a potentially long list.
      this.listen(this.$.checkbox, 'keydown', 'onCheckboxKeydown_');
    });
  },

  /** @override */
  detached() {
    this.unlisten(this.$.checkbox, 'keydown', 'onCheckboxKeydown_');
  },

  focusOnMenuButton() {
    focusWithoutInk(this.$['menu-button']);
  },

  /** @param {!KeyboardEvent} e */
  onCheckboxKeydown_(e) {
    if (e.shiftKey && e.key === 'Tab') {
      this.focus();
    }
  },

  /**
   * Toggle item selection whenever the checkbox or any non-interactive part
   * of the item is clicked.
   * @param {MouseEvent} e
   * @private
   */
  onItemClick_(e) {
    for (let i = 0; i < e.path.length; i++) {
      const elem = e.path[i];
      if (elem.id !== 'checkbox' &&
          (elem.nodeName === 'A' || elem.nodeName === 'CR-ICON-BUTTON')) {
        return;
      }
    }

    if (this.selectionNotAllowed_) {
      return;
    }

    this.$.checkbox.focus();
    this.fire('history-checkbox-select', {
      index: this.index,
      shiftKey: e.shiftKey,
    });
  },

  /**
   * This is bound to mouse/keydown instead of click/press because this
   * has to fire before onCheckboxChange_. If we bind it to click/press,
   * it might trigger out of desired order.
   *
   * @param {!Event} e
   * @private
   */
  onCheckboxClick_(e) {
    this.isShiftKeyDown_ = e.shiftKey;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onCheckboxChange_(e) {
    this.fire('history-checkbox-select', {
      index: this.index,
      // If the user clicks or press enter/space key, oncheckboxClick_ will
      // trigger before this function, so a shift-key might be recorded.
      shiftKey: this.isShiftKeyDown_,
    });

    this.isShiftKeyDown_ = false;
  },

  /**
   * @param {MouseEvent} e
   * @private
   */
  onItemMousedown_(e) {
    // Prevent shift clicking a checkbox from selecting text.
    if (e.shiftKey) {
      e.preventDefault();
    }
  },

  /**
   * @private
   * @return {string}
   */
  getEntrySummary_() {
    const item = this.item;
    return loadTimeData.getStringF(
        'entrySummary',
        this.isCardStart || this.isCardEnd ?
            this.cardTitle_(
                this.numberOfItems, item.dateRelativeDay, this.searchTerm) :
            '',
        item.dateTimeOfDay,
        item.starred ? loadTimeData.getString('bookmarked') : '', item.title,
        item.domain);
  },

  /**
   * The first and last rows of a card have a described-by field pointing to
   * the date header, to make sure users know if they have jumped between cards
   * when navigating up or down with the keyboard.
   * @private
   * @return {string}
   */
  getAriaDescribedByForHeading_() {
    return this.isCardStart || this.isCardEnd ? 'date-accessed' : '';
  },

  /**
   * @param {boolean} selected
   * @return {string}
   * @private
   */
  getAriaChecked_(selected) {
    return selected ? 'true' : 'false';
  },

  /**
   * Remove bookmark of current item when bookmark-star is clicked.
   * @private
   */
  onRemoveBookmarkTap_() {
    if (!this.item.starred) {
      return;
    }

    if (this.$$('#bookmark-star') === this.root.activeElement) {
      focusWithoutInk(this.$['menu-button']);
    }

    const browserService = BrowserService.getInstance();
    browserService.removeBookmark(this.item.url);
    browserService.recordAction('BookmarkStarClicked');

    this.fire('remove-bookmark-stars', this.item.url);
  },

  /**
   * Fires a custom event when the menu button is clicked. Sends the details
   * of the history item and where the menu should appear.
   */
  onMenuButtonTap_(e) {
    this.fire('open-menu', {
      target: e.target,
      index: this.index,
      item: this.item,
    });

    // Stops the 'click' event from closing the menu when it opens.
    e.stopPropagation();
  },

  /**
   * Record metrics when a result is clicked.
   * @private
   */
  onLinkClick_() {
    const browserService = BrowserService.getInstance();
    browserService.recordAction('EntryLinkClick');

    if (this.searchTerm) {
      browserService.recordAction('SearchResultClick');
    }

    if (this.index === undefined) {
      return;
    }

    const ageInDays = Math.ceil(
        (new Date() - new Date(this.item.time)) / 1000 /* s/ms */ /
        60 /* m/s */ / 60 /* h/m */ / 24 /* d/h */);

    browserService.recordHistogram(
        'HistoryPage.ClickPosition', Math.min(this.index, UMA_MAX_BUCKET_VALUE),
        UMA_MAX_BUCKET_VALUE);

    browserService.recordHistogram(
        'HistoryPage.ClickAgeInDays', Math.min(ageInDays, UMA_MAX_BUCKET_VALUE),
        UMA_MAX_BUCKET_VALUE);

    if (this.index <= UMA_MAX_SUBSET_BUCKET_VALUE) {
      browserService.recordHistogram(
          'HistoryPage.ClickPositionSubset', this.index,
          UMA_MAX_SUBSET_BUCKET_VALUE);
    }

    if (ageInDays <= UMA_MAX_SUBSET_BUCKET_VALUE) {
      browserService.recordHistogram(
          'HistoryPage.ClickAgeInDaysSubset', ageInDays,
          UMA_MAX_SUBSET_BUCKET_VALUE);
    }
  },

  onLinkRightClick_() {
    BrowserService.getInstance().recordAction('EntryLinkRightClick');
  },

  /**
   * Set the favicon image, based on the URL of the history item.
   * @private
   */
  itemChanged_() {
    this.$.icon.style.backgroundImage = getFaviconForPageURL(
        this.item.url, this.item.isUrlInRemoteUserData,
        this.item.remoteIconUrlForUma);
    this.listen(this.$['time-accessed'], 'mouseover', 'addTimeTitle_');
  },

  /**
   * @param {number} numberOfItems The number of items in the card.
   * @param {string} historyDate Date of the current result.
   * @param {string} search The search term associated with these results.
   * @return {string} The title for this history card.
   * @private
   */
  cardTitle_(numberOfItems, historyDate, search) {
    if (this.item === undefined) {
      return '';
    }

    if (!search) {
      return this.item.dateRelativeDay;
    }
    return searchResultsTitle(numberOfItems, search);
  },

  /** @private */
  addTimeTitle_() {
    const el = this.$['time-accessed'];
    el.setAttribute('title', new Date(this.item.time).toString());
    this.unlisten(el, 'mouseover', 'addTimeTitle_');
  },

  /**
   * @param {!Element} sampleElement An element to find an equivalent for.
   * @return {?Element} An equivalent element to focus, or null to use the
   *     default element.
   */
  getCustomEquivalent(sampleElement) {
    return sampleElement.getAttribute('focus-type') === 'star' ? this.$.link :
                                                                 null;
  },
});

/**
 * @param {number} numberOfResults
 * @param {string} searchTerm
 * @return {string} The title for a page of search results.
 */
export function searchResultsTitle(numberOfResults, searchTerm) {
  const resultId = numberOfResults === 1 ? 'searchResult' : 'searchResults';
  return loadTimeData.getStringF(
      'foundSearchResults', numberOfResults, loadTimeData.getString(resultId),
      searchTerm);
}
