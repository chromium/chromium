// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './realbox_match.js';
import './realbox_dropdown_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';

import {MetricsReporterImpl} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {AutocompleteMatch, AutocompleteResult, PageHandlerInterface} from '../omnibox.mojom-webui.js';
import {decodeString16} from '../utils.js';

import {RealboxBrowserProxy} from './realbox_browser_proxy.js';
import {getTemplate} from './realbox_dropdown.html.js';

const CHAR_TYPED_TO_PAINT = 'Realbox.CharTypedToRepaintLatency.ToPaint';
const RESULT_CHANGED_TO_PAINT = 'Realbox.ResultChangedToRepaintLatency.ToPaint';

export interface RealboxDropdownElement {
  $: {
    groups: DomRepeat,
    selector: IronSelectorElement,
  };
}

// A dropdown element that contains autocomplete matches. Provides an API for
// the embedder (i.e., <ntp-realbox>) to change the selection.
export class RealboxDropdownElement extends PolymerElement {
  static get is() {
    return 'ntp-realbox-dropdown';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      result: {
        type: Object,
      },

      /** Whether the realbox should have rounded corners. */
      roundCorners: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('roundCorners'),
        reflectToAttribute: true,
      },

      /** Index of the selected match. */
      selectedMatchIndex: {
        type: Number,
        value: -1,
        notify: true,
      },

      //========================================================================
      // Private properties
      //========================================================================

      /** The list of suggestion group IDs matches belong to. */
      groupIds_: {
        type: Array,
        computed: `computeGroupIds_(result)`,
      },

      /** The list of suggestion group IDs whose matches should be hidden. */
      hiddenGroupIds_: {
        type: Array,
        computed: `computeHiddenGroupIds_(result)`,
      },

      /** The list of selectable match elements. */
      selectableMatchElements_: {
        type: Array,
        value: () => [],
      },
    };
  }

  result: AutocompleteResult;
  roundCorners: boolean;
  selectedMatchIndex: number;
  private groupIds_: number[];
  private hiddenGroupIds_: number[];
  private selectableMatchElements_: Element[];

  private pageHandler_: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler_ = RealboxBrowserProxy.getInstance().handler;
  }

  //============================================================================
  // Public methods
  //============================================================================

  /** Unselects the currently selected match, if any. */
  unselect() {
    this.selectedMatchIndex = -1;
  }

  /** Focuses the selected match, if any. */
  focusSelected() {
    if (this.$.selector.selectedItem) {
      (this.$.selector.selectedItem as HTMLElement).focus();
    }
  }

  /** Selects the first match. */
  selectFirst() {
    this.selectedMatchIndex = 0;
  }

  /** Selects the match at the given index. */
  selectIndex(index: number) {
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

  /** Selects the last match. */
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

  private onResultRepaint_() {
    this.dispatchEvent(new CustomEvent('result-repaint', {
      bubbles: true,
      composed: true,
      detail: window.performance.now(),
    }));

    const metricsReporter = MetricsReporterImpl.getInstance();
    metricsReporter.measure('CharTyped')
        .then(duration => {
          metricsReporter.umaReportTime(CHAR_TYPED_TO_PAINT, duration);
        })
        .then(() => {
          metricsReporter.clearMark('CharTyped');
        })
        .catch(() => {});  // Fail silently if 'CharTyped' is not marked.

    metricsReporter.measure('ResultChanged')
        .then(duration => {
          metricsReporter.umaReportTime(RESULT_CHANGED_TO_PAINT, duration);
        })
        .then(() => {
          metricsReporter.clearMark('ResultChanged');
        })
        .catch(() => {});  // Fail silently if 'ResultChanged' is not marked.
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onHeaderFocusin_() {
    this.dispatchEvent(new CustomEvent('header-focusin', {
      bubbles: true,
      composed: true,
    }));
  }

  private onHeaderClick_(e: Event) {
    const groupId =
        Number.parseInt((e.currentTarget as HTMLElement).dataset['id']!, 10);

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

  private onToggleButtonMouseDown_(e: Event) {
    e.preventDefault();  // Prevents default browser action (focus).
  }

  //============================================================================
  // Helpers
  //============================================================================

  /**
   * @returns Index of the match in the autocomplete result. Passed to the match
   *     so it knows abut its position in the list of matches.
   */
  private matchIndex_(match: AutocompleteMatch): number {
    if (!this.result || !this.result.matches) {
      return -1;
    }

    return this.result.matches.indexOf(match);
  }

  private computeGroupIds_(): number[] {
    if (!this.result || !this.result.matches) {
      return [];
    }

    // Extract the suggestion group IDs from autocomplete matches and return the
    // unique IDs while preserving the order. Autocomplete matches are the
    // ultimate source of truth for suggestion groups IDs matches belong to.
    return [...new Set(
        this.result.matches.map(match => match.suggestionGroupId))];
  }

  private computeHiddenGroupIds_(): number[] {
    if (!this.result) {
      return [];
    }

    return Object.keys(this.result.suggestionGroupsMap)
        .map(groupId => Number.parseInt(groupId, 10))
        .filter(groupId => this.result.suggestionGroupsMap[groupId].hidden);
  }

  /**
   * @returns The filter function to filter matches that belong to the given
   *     suggestion group ID.
   */
  private computeMatchBelongsToGroup_(groupId: number):
      (match: AutocompleteMatch) => boolean {
    return (match) => {
      return match.suggestionGroupId === groupId;
    };
  }

  /**
   * @returns Whether the given suggestion group ID has a header.
   */
  private groupHasHeader_(groupId: number): boolean {
    return !!this.headerForGroup_(groupId);
  }

  /**
   * @returns Whether matches with the given suggestion group ID should be
   *     hidden.
   */
  private groupIsHidden_(groupId: number): boolean {
    return this.hiddenGroupIds_.indexOf(groupId) !== -1;
  }

  /**
   * @returns The header for the given suggestion group ID.
   */
  private headerForGroup_(groupId: number): string {
    return (this.result && this.result.suggestionGroupsMap &&
            this.result.suggestionGroupsMap[groupId]) ?
        decodeString16(this.result.suggestionGroupsMap[groupId].header) :
        '';
  }

  /**
   * @returns Tooltip for suggestion group show/hide toggle button.
   */
  private toggleButtonTitleForGroup_(groupId: number): string {
    if (!this.groupHasHeader_(groupId)) {
      return '';
    }
    return loadTimeData.getString(
        this.groupIsHidden_(groupId) ? 'showSuggestions' : 'hideSuggestions');
  }

  /**
   * @returns A11y label for suggestion group show/hide toggle button.
   */
  private toggleButtonA11yLabelForGroup_(groupId: number): string {
    if (!this.groupHasHeader_(groupId)) {
      return '';
    }
    return !this.groupIsHidden_(groupId) ?
        decodeString16(
            this.result.suggestionGroupsMap[groupId].hideGroupA11yLabel) :
        decodeString16(
            this.result.suggestionGroupsMap[groupId].showGroupA11yLabel);
  }

  private expandIconNameForGroup_(groupId: number): string {
    if (!this.groupHasHeader_(groupId)) {
      return '';
    }
    return this.groupIsHidden_(groupId) ? 'icon-expand-more' :
                                          'icon-expand-less';
  }
}

customElements.define(RealboxDropdownElement.is, RealboxDropdownElement);
