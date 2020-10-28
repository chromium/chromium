// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './realbox_button.js';
import './realbox_match.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {decodeString16} from './utils.js';

// A dropdown element that contains autocomplete matches. Provides an API for
// the embedder (i.e., <ntp-realbox>) to change the selection.
class RealboxDropdownElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-dropdown';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      /**
       * @type {!search.mojom.AutocompleteResult}
       */
      result: {
        type: Object,
      },

      /**
       * Index of the selected match.
       * @type {number}
       */
      selectedMatchIndex: {
        type: Number,
        value: -1,
        notify: true,
      },

      /**
       * @type {!newTabPage.mojom.SearchBoxTheme}
       */
      theme: {
        type: Object,
        observer: 'onThemeChange_',
      },

      //========================================================================
      // Private properties
      //========================================================================

      /**
       * The list of suggestion group IDs matches belong to.
       * @type {!Array<number>}
       * @private
       */
      groupIds_: {
        type: Array,
        computed: `computeGroupIds_(result)`,
      },

      /**
       * The list of suggestion group IDs whose matches should be hidden.
       * @type {!Array<number>}
       * @private
       */
      hiddenGroupIds_: {
        type: Array,
        computed: `computeHiddenGroupIds_(result)`,
      },

      /**
       * The list of selectable match elements.
       * @type {!Array<!Element>}
       * @private
       */
      selectableMatchElements_: {
        type: Array,
        value: () => [],
      },
    };
  }

  constructor() {
    super();
    /** @private {!newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    /** @private {?number} */
    this.autocompleteMatchImageAvailableListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.autocompleteMatchImageAvailableListenerId_ =
        this.callbackRouter_.autocompleteMatchImageAvailable.addListener(
            this.onAutocompleteMatchImageAvailable_.bind(this));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter_.removeListener(
        assert(this.autocompleteMatchImageAvailableListenerId_));
  }

  //============================================================================
  // Public methods
  //============================================================================

  /**
   * Unselects the currently selected match, if any.
   */
  unselect() {
    this.selectedMatchIndex = -1;
  }

  /**
   * Focuses the selected match, if any.
   */
  focusSelected() {
    if (this.$.selector.selectedItem) {
      this.$.selector.selectedItem.focus();
    }
  }

  /**
   * Selects the first match.
   */
  selectFirst() {
    this.selectedMatchIndex = 0;
  }

  /**
   * Selects the match at the given index.
   * @param {number} index
   */
  selectIndex(index) {
    this.selectedMatchIndex = index;
  }

  /**
   * Selects the previous match with respect to the currently selected one.
   * Selects the last match if the first one is currently selected.
   */
  selectPrevious() {
    this.selectedMatchIndex = this.selectedMatchIndex - 1 >= 0 ?
        this.selectedMatchIndex - 1 :
        this.selectableMatchElements_.length - 1;
  }

  /**
   * Selects the last match.
   */
  selectLast() {
    this.selectedMatchIndex = this.selectableMatchElements_.length - 1;
  }

  /**
   * Selects the next match with respect to the currently selected one.
   * Selects the first match if the last one is currently selected.
   */
  selectNext() {
    this.selectedMatchIndex =
        this.selectedMatchIndex + 1 < this.selectableMatchElements_.length ?
        this.selectedMatchIndex + 1 :
        0;
  }

  //============================================================================
  // Callbacks
  //============================================================================

  /**
   * @param {number} matchIndex match index
   * @param {!url.mojom.Url} url match imageUrl or destinationUrl.
   * @param {string} dataUrl match image or favicon content in in base64 encoded
   *     Data URL format.
   * @private
   */
  onAutocompleteMatchImageAvailable_(matchIndex, url, dataUrl) {
    if (!this.result || !this.result.matches) {
      return;
    }

    const match = this.result.matches[matchIndex];
    if (!match) {
      return;
    }

    // Set image or favicon content of the match, if applicable.
    if (match.destinationUrl.url === url.url) {
      this.set(`result.matches.${matchIndex}.faviconDataUrl`, dataUrl);
    } else if (match.imageUrl === url.url) {
      this.set(`result.matches.${matchIndex}.imageDataUrl`, dataUrl);
    }
  }

  /**
   * @private
   */
  onResultRepaint_() {
    this.dispatchEvent(new CustomEvent('result-repaint', {
      bubbles: true,
      composed: true,
      detail: window.performance.now(),
    }));
  }

  /**
   * @private
   */
  onThemeChange_() {
    if (!loadTimeData.getBoolean('realboxMatchOmniboxTheme')) {
      return;
    }

    const icon = assert(this.theme.icon);
    // Icon's background color in a hovered context (0x29 == .16).
    // TODO(crbug.com/1041129): Share this with the Omnibox.
    const iconBgHovered = {value: icon.value & 0x29ffffff};

    const iconSelected = assert(this.theme.iconSelected);
    // Icon's background color in a focused context (0x52 == .32).
    // TODO(crbug.com/1041129): Share this with the Omnibox.
    const iconBgFocused = {value: iconSelected.value & 0x52ffffff};

    this.updateStyles({
      '--search-box-icon-bg-focused': skColorToRgba(iconBgFocused),
      '--search-box-icon-bg-hovered': skColorToRgba(iconBgHovered),
      '--search-box-icon-selected': skColorToRgba(iconSelected),
      '--search-box-icon': skColorToRgba(icon),
      '--search-box-results-bg-hovered':
          skColorToRgba(assert(this.theme.resultsBgHovered)),
      '--search-box-results-bg-selected':
          skColorToRgba(assert(this.theme.resultsBgSelected)),
      '--search-box-results-bg': skColorToRgba(assert(this.theme.resultsBg)),
      '--search-box-results-dim-selected':
          skColorToRgba(assert(this.theme.resultsDimSelected)),
      '--search-box-results-dim': skColorToRgba(assert(this.theme.resultsDim)),
      '--search-box-results-text-selected':
          skColorToRgba(assert(this.theme.resultsTextSelected)),
      '--search-box-results-text':
          skColorToRgba(assert(this.theme.resultsText)),
      '--search-box-results-url-selected':
          skColorToRgba(assert(this.theme.resultsUrlSelected)),
      '--search-box-results-url': skColorToRgba(assert(this.theme.resultsUrl)),
    });
  }

  //============================================================================
  // Event handlers
  //============================================================================

  /**
   * @private
   */
  onHeaderFocusin_() {
    this.dispatchEvent(new CustomEvent('header-focusin', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onHeaderClick_(e) {
    const groupId = Number(e.currentTarget.dataset.id);

    // Tell the backend to toggle visibility of the given suggestion group ID.
    this.pageHandler_.toggleSuggestionGroupIdVisibility(groupId);

    // Hide/Show matches with the given suggestion group ID.
    const index = this.hiddenGroupIds_.indexOf(groupId);
    if (index === -1) {
      this.push('hiddenGroupIds_', groupId);
    } else {
      this.splice('hiddenGroupIds_', index, 1);
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onToggleButtonKeydown_(e) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }

    // Simulate a click so that it gets handled by |onHeaderClick_|.
    e.target.click();
    e.preventDefault();  // Prevents default browser action.
  }

  //============================================================================
  // Helpers
  //============================================================================

  /**
   * @returns {number} Index of the match in the autocomplete result. Passed to
   *     the match so it knows abut its position in the list of matches.
   * @private
   */
  matchIndex_(match) {
    if (!this.result || !this.result.matches) {
      return -1;
    }

    return this.result.matches.indexOf(match);
  }

  /**
   * @returns {!Array<number>}
   * @private
   */
  computeGroupIds_() {
    if (!this.result || !this.result.matches) {
      return [];
    }

    // Extract the suggestion group IDs from autocomplete matches and return the
    // unique IDs while preserving the order. Autocomplete matches are the
    // ultimate source of truth for suggestion groups IDs matches belong to.
    return [...new Set(
        this.result.matches.map(match => match.suggestionGroupId))];
  }

  /**
   * @returns {!Array<number>}
   * @private
   */
  computeHiddenGroupIds_() {
    if (!this.result) {
      return [];
    }

    return Object.keys(this.result.suggestionGroupsMap)
        .map(groupId => Number(groupId))
        .filter((groupId => {
                  return this.result.suggestionGroupsMap[groupId].hidden;
                }).bind(this));
  }

  /**
   * @param {number} groupId
   * @returns {!function(!search.mojom.AutocompleteMatch):boolean} The filter
   *     function to filter matches that belong to the given suggestion group
   *     ID.
   * @private
   */
  computeMatchBelongsToGroup_(groupId) {
    return (match) => {
      return match.suggestionGroupId === groupId;
    };
  }

  /**
   * @param {number} groupId
   * @returns {boolean} Whether the given suggestion group ID has a header.
   * @private
   */
  groupHasHeader_(groupId) {
    return !!this.headerForGroup_(groupId);
  }

  /**
   * @param {number} groupId
   * @returns {boolean} Whether matches with the given suggestion group ID
   *     should be hidden.
   * @private
   */
  groupIsHidden_(groupId) {
    return this.hiddenGroupIds_.indexOf(groupId) !== -1;
  }

  /**
   * @param {number} groupId
   * @returns {string} The header for the given suggestion group ID.
   * @private
   * @suppress {checkTypes}
   */
  headerForGroup_(groupId) {
    return (this.result && this.result.suggestionGroupsMap &&
            this.result.suggestionGroupsMap[groupId]) ?
        decodeString16(this.result.suggestionGroupsMap[groupId].header) :
        '';
  }

  /**
   * @param {number} groupId
   * @returns {string} Tooltip for suggestion group show/hide toggle button.
   * @private
   */
  toggleButtonTitleForGroup_(groupId) {
    if (!this.groupHasHeader_(groupId)) {
      return '';
    }
    return loadTimeData.getString(
        this.groupIsHidden_(groupId) ? 'showSuggestions' : 'hideSuggestions');
  }

  /**
   * @param {number} groupId
   * @returns {string} A11y label for suggestion group show/hide toggle button.
   * @private
   */
  toggleButtonA11yLabelForGroup_(groupId) {
    if (!this.groupHasHeader_(groupId)) {
      return '';
    }
    return loadTimeData.substituteString(
        loadTimeData.getString(
            this.groupIsHidden_(groupId) ? 'showSection' : 'hideSection'),
        this.headerForGroup_(groupId));
  }
}

customElements.define(RealboxDropdownElement.is, RealboxDropdownElement);
