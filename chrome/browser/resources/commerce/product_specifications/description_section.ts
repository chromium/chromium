// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './description_citation.js';
import './empty_section.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './description_section.css.js';
import {getHtml} from './description_section.html.js';
import type {ProductSpecificationsDescriptionText} from './shopping_service.mojom-webui.js';

interface Attribute {
  label: string;
  value: string;
}

export interface ProductDescription {
  attributes: Attribute[];
  summary: ProductSpecificationsDescriptionText[];
}

export interface DescriptionSectionElement {
  $: {
    attributes: HTMLElement,
    summary: HTMLElement,
  };
}

export class DescriptionSectionElement extends CrLitElement {
  static get is() {
    return 'description-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    this.citationCount = (this.description.summary || [])
                             .map(summary => summary.urls.length)
                             .reduce((count, current) => count + current, 0);
    super.willUpdate(changedProperties);
  }

  static override get properties() {
    return {
      citationCount: {type: Number},
      description: {type: Object},
      productName: {type: String},
    };
  }

  accessor description: ProductDescription = {
    attributes: [],
    summary: [],
  };
  accessor productName: string = '';
  accessor citationCount: number = 0;

  protected computeCitationIndex_(summaryIndex: number, urlIndex: number):
      number {
    // Citations should start from 1.
    let citationIndex = 1;
    for (let i = 0; i < summaryIndex; i++) {
      citationIndex += this.description.summary[i]?.urls.length || 0;
    }
    return citationIndex + urlIndex;
  }

  protected summaryIsEmpty_(summary: ProductSpecificationsDescriptionText[]):
      boolean {
    return summary.length === 0 || summary.every(summary => {
      return summary.text.length === 0 || summary.text === 'N/A';
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'description-section': DescriptionSectionElement;
  }
}

customElements.define(DescriptionSectionElement.is, DescriptionSectionElement);
