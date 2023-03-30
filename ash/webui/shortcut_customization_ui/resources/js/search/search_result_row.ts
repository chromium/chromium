// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../text_accelerator.js';

import {FocusRowMixin} from 'chrome://resources/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {mojoString16ToString} from '../mojo_utils.js';
import {LayoutStyle, MojoAcceleratorInfo, MojoSearchResult, StandardAcceleratorInfo, TextAcceleratorInfo, TextAcceleratorPart} from '../shortcut_types.js';
import {getModifiersForAcceleratorInfo, isStandardAcceleratorInfo, isTextAcceleratorInfo} from '../shortcut_utils.js';
import {TextAcceleratorElement} from '../text_accelerator.js';

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

      /** Whether the search result row is selected. */
      selected: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  searchResult: MojoSearchResult;
  selected: boolean;

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  private getSearchResultDescription(): string {
    return mojoString16ToString(
        this.searchResult.acceleratorLayoutInfo.description);
  }

  private isStandardLayout(): boolean {
    return this.searchResult.acceleratorLayoutInfo.style ===
        LayoutStyle.kDefault;
  }

  private isTextLayout(): boolean {
    return !this.isStandardLayout();
  }

  private getTextAcceleratorParts(): TextAcceleratorPart[] {
    assert(isTextAcceleratorInfo(this.searchResult.acceleratorInfos[0]));
    return TextAcceleratorElement.getTextAcceleratorParts(
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
}

declare global {
  interface HTMLElementTagNameMap {
    'search-result-row': SearchResultRowElement;
  }
}

customElements.define(SearchResultRowElement.is, SearchResultRowElement);
