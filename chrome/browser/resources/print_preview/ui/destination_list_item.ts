// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';
import '/strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {removeHighlights} from 'chrome://resources/js/search_highlight_utils.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Destination} from '../data/destination.js';

import {getCss} from './destination_list_item.css.js';
import {getHtml} from './destination_list_item.html.js';
import {updateHighlights} from './highlight_utils.js';

export class PrintPreviewDestinationListItemElement extends CrLitElement {
  static get is() {
    return 'print-preview-destination-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      destination: {type: Object},
      searchQuery: {type: Object},
      searchHint_: {type: String},
    };
  }

  accessor destination: Destination|null = null;
  accessor searchQuery: RegExp|null = null;
  protected accessor searchHint_: string = '';

  private highlights_: HTMLElement[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('destination') ||
        changedProperties.has('searchQuery')) {
      this.updateSearchHint_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('destination')) {
      this.onDestinationPropertiesChange_();
    }

    if (changedProperties.has('destination') ||
        changedProperties.has('searchQuery')) {
      removeHighlights(this.highlights_);
      this.highlights_ = updateHighlights(this, this.searchQuery, new Map());
    }
  }

  private onDestinationPropertiesChange_() {
    if (this.destination === null) {
      return;
    }

    this.title = this.destination.displayName;
    if (this.destination.isExtension) {
      const icon =
          this.shadowRoot.querySelector<HTMLElement>('.extension-icon');
      assert(icon);
      icon.style.backgroundImage = 'image-set(' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/24/1) 1x,' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/48/1) 2x)';
    }
  }

  private updateSearchHint_() {
    if (this.destination === null) {
      return;
    }

    const matches = !this.searchQuery ?
        [] :
        this.destination.extraPropertiesToMatch.filter(
            p => p.match(this.searchQuery!));
    this.searchHint_ = matches.length === 0 ?
        (this.destination.extraPropertiesToMatch.find(p => !!p) || '') :
        matches.join(' ');
  }

  protected getExtensionPrinterTooltip_(): string {
    assert(this.destination);

    if (!this.destination.isExtension) {
      return '';
    }
    return loadTimeData.getStringF(
        'extensionDestinationIconTooltip', this.destination.extensionName);
  }
}

export type DestinationListItemElement = PrintPreviewDestinationListItemElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-destination-list-item':
        PrintPreviewDestinationListItemElement;
  }
}

customElements.define(
    PrintPreviewDestinationListItemElement.is,
    PrintPreviewDestinationListItemElement);
