// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './description_citation.js';

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
      description: {type: Object},
      productName: {type: String},
    };
  }

  description: ProductDescription = {
    attributes: [],
    summary: [],
  };
  productName: string = '';
  citationCount: number = 0;

  protected computeCitationIndex_(summaryIndex: number, urlIndex: number):
      number {
    // Citations should start from 1.
    let citationIndex = 1;
    for (let i = 0; i < summaryIndex; i++) {
      citationIndex += this.description.summary[i].urls.length;
    }
    return citationIndex + urlIndex;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'description-section': DescriptionSectionElement;
  }
}

customElements.define(DescriptionSectionElement.is, DescriptionSectionElement);
