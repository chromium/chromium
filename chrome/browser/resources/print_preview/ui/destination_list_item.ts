// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';
import './destination_list_item_style.css.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {removeHighlights} from 'chrome://resources/js/search_highlight_utils.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Destination} from '../data/destination.js';

import {getTemplate} from './destination_list_item.html.js';
import {updateHighlights} from './highlight_utils.js';

export class PrintPreviewDestinationListItemElement extends PolymerElement {
  static get is() {
    return 'print-preview-destination-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      destination: Object,
      searchQuery: Object,
      searchHint_: String,
    };
  }

  static get observers() {
    return [
      'onDestinationPropertiesChange_(' +
          'destination.displayName, destination.isExtension)',
      'updateHighlightsAndHint_(destination, searchQuery)',
    ];
  }

  destination: Destination;
  searchQuery: RegExp|null;
  private destinationIcon_: string;
  private searchHint_: string;

  private highlights_: HTMLElement[] = [];

  private onDestinationPropertiesChange_() {
    this.title = this.destination.displayName;
    if (this.destination.isExtension) {
      const icon =
          this.shadowRoot!.querySelector<HTMLElement>('.extension-icon');
      assert(icon);
      icon.style.backgroundImage = 'image-set(' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/24/1) 1x,' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/48/1) 2x)';
    }
  }

  private updateHighlightsAndHint_() {
    this.updateSearchHint_();
    removeHighlights(this.highlights_);
    this.highlights_ = updateHighlights(this, this.searchQuery, new Map());
  }

  private updateSearchHint_() {
    const matches = !this.searchQuery ?
        [] :
        this.destination.extraPropertiesToMatch.filter(
            p => p.match(this.searchQuery!));
    this.searchHint_ = matches.length === 0 ?
        (this.destination.extraPropertiesToMatch.find(p => !!p) || '') :
        matches.join(' ');
  }

  private getExtensionPrinterTooltip_(): string {
    if (!this.destination.isExtension) {
      return '';
    }
    return loadTimeData.getStringF(
        'extensionDestinationIconTooltip', this.destination.extensionName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-list-item':
        PrintPreviewDestinationListItemElement;
  }
}

customElements.define(
    PrintPreviewDestinationListItemElement.is,
    PrintPreviewDestinationListItemElement);
