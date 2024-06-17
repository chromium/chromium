// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import '../text_accelerator.js';
import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {FocusRowMixin} from 'chrome://resources/ash/common/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from '../accelerator_lookup_manager.js';
import {Router} from '../router.js';
import {LayoutStyle, MetaKey, MojoAcceleratorInfo, MojoSearchResult, StandardAcceleratorInfo, TextAcceleratorInfo, TextAcceleratorPart} from '../shortcut_types.js';
import {getAriaLabelForStandardAccelerators, getAriaLabelForTextAccelerators, getModifiersForAcceleratorInfo, getTextAcceleratorParts, getURLForSearchResult, isStandardAcceleratorInfo, isTextAcceleratorInfo} from '../shortcut_utils.js';

import {getBoldedDescription} from './search_result_bolding.js';
import {getTemplate} from './search_result_row.html.js';

/**
 * @fileoverview
 * 'search-result-row' is the container for one search result.
 */

const SearchResultRowElementBase = FocusRowMixin(I18nMixin(PolymerElement));

export class SearchResultRowElement extends SearchResultRowElementBase {
  static get is(): string {
    return 'search-result-row';
  }

  static get properties(): PolymerElementProperties {
    return {
      searchResult: {
        type: Object,
      },

      /** The query used to fetch this result. */
      searchQuery: {
        type: String,
      },

      /** Whether the search result row is selected. */
      selected: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'makeA11yAnnouncementIfSelectedAndUnfocused',
      },

      /** Aria label for the row. */
      ariaLabel: {
        type: String,
        computed: 'computeAriaLabel(searchResult)',
        reflectToAttribute: true,
      },

      /** Number of rows in the list this row is part of. */
      listLength: Number,

      /** The meta key on the keyboard to display to the user. */
      metaKey: Object,
    };
  }

  override ariaLabel: string;
  listLength: number;
  searchResult: MojoSearchResult;
  searchQuery: string;
  selected: boolean;
  metaKey: MetaKey = MetaKey.kSearch;
  private lookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.metaKey = this.lookupManager.getMetaKeyToDisplay();
  }

  private isNoShortcutAssigned(): boolean {
    return this.searchResult.acceleratorInfos.length === 0;
  }

  private isStandardLayout(): boolean {
    return !this.isNoShortcutAssigned() &&
        this.searchResult.acceleratorLayoutInfo.style === LayoutStyle.kDefault;
  }

  private isTextLayout(): boolean {
    return !this.isNoShortcutAssigned() && !this.isStandardLayout();
  }

  private getTextAcceleratorParts(): TextAcceleratorPart[] {
    assert(isTextAcceleratorInfo(this.searchResult.acceleratorInfos[0]));
    return getTextAcceleratorParts(
        this.searchResult.acceleratorInfos as TextAcceleratorInfo[]);
  }

  private getStandardAcceleratorInfos(): StandardAcceleratorInfo[] {
    assert(this.isStandardLayout());
    // Convert MojoAcceleratorInfo from the search result into the converted
    // AcceleratorInfo type used throughout the app.
    // In practice, this means that we need to convert the String16 keyDisplay
    // property on each accelerator into strings.
    return this.searchResult.acceleratorInfos.map(
        (acceleratorInfo: MojoAcceleratorInfo) => {
          assert(isStandardAcceleratorInfo(acceleratorInfo));
          return {
            ...acceleratorInfo,
            layoutProperties: {
              ...acceleratorInfo.layoutProperties,
              standardAccelerator: {
                ...acceleratorInfo.layoutProperties.standardAccelerator,
                keyDisplay:
                    mojoString16ToString(acceleratorInfo.layoutProperties
                                             .standardAccelerator.keyDisplay),
              },
            },
            // Cast to StandardAcceleratorInfo here since that type uses strings
            // instead of String16s, and we couldn't perform the assignment
            // above if it were still a MojoAcceleratorInfo.
          } as StandardAcceleratorInfo;
        });
  }

  private getStandardAcceleratorModifiers(
      acceleratorInfo: StandardAcceleratorInfo): string[] {
    return getModifiersForAcceleratorInfo(acceleratorInfo);
  }

  private getStandardAcceleratorKey(acceleratorInfo: StandardAcceleratorInfo):
      string {
    return acceleratorInfo.layoutProperties.standardAccelerator.keyDisplay;
  }

  // If true, show "or" after the AcceleratorInfo at the given index.
  private shouldShowTextDivider(indexOfAcceleratorInfo: number): boolean {
    return indexOfAcceleratorInfo !==
        this.searchResult.acceleratorInfos.length - 1;
  }

  /**
   * Only relevant when the focus-row-control is focus()ed. This keypress
   * handler specifies that pressing 'Enter' should cause a route change.
   */
  private onKeyPress(e: KeyboardEvent): void {
    if (e.key === 'Enter') {
      e.stopPropagation();
      this.onSearchResultSelected();
    }
  }

  /**
   * Navigate to a search result route based on the search result.
   */
  onSearchResultSelected(): void {
    Router.getInstance().navigateTo(getURLForSearchResult(this.searchResult));
    this.dispatchEvent(new CustomEvent(
        'navigated-to-result-route', {bubbles: true, composed: true}));
  }

  private getSearchResultDescriptionInnerHtml(): TrustedHTML {
    return getBoldedDescription(
        mojoString16ToString(
            this.searchResult.acceleratorLayoutInfo.description),
        this.searchQuery);
  }

  /**
   * @return Aria label string for ChromeVox to verbalize.
   */
  private computeAriaLabel(): string {
    const description = mojoString16ToString(
        this.searchResult.acceleratorLayoutInfo.description);
    let searchResultText;

    if (this.isNoShortcutAssigned()) {
      searchResultText = `${description}, ${this.i18n('noShortcutAssigned')}`;
    } else if (this.isStandardLayout()) {
      searchResultText = `${description}, ${
          getAriaLabelForStandardAccelerators(
              this.getStandardAcceleratorInfos(),
              this.i18n('acceleratorTextDivider'))}`;
    } else {
      searchResultText = `${description}, ${
          getAriaLabelForTextAccelerators(
              this.searchResult.acceleratorInfos as TextAcceleratorInfo[])}`;
    }

    return this.i18n(
        'searchResultSelectedAriaLabel', this.focusRowIndex + 1,
        this.listLength, searchResultText);
  }

  private makeA11yAnnouncementIfSelectedAndUnfocused(): void {
    if (!this.selected || this.lastFocused) {
      // Do not alert the user if the result is not selected, or
      // the list is focused, defer to aria tags instead.
      return;
    }

    // The selected item is normally not focused when selected, the
    // selected search result should be verbalized as it changes.
    getAnnouncerInstance().announce(this.ariaLabel);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-result-row': SearchResultRowElement;
  }
}

customElements.define(SearchResultRowElement.is, SearchResultRowElement);
