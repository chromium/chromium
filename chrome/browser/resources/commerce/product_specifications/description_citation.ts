// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';

import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './description_citation.css.js';
import {getHtml} from './description_citation.html.js';
import {getAbbreviatedUrl} from './utils.js';

export interface DescriptionCitationElement {
  $: {
    citation: HTMLElement,
    tooltip: CrTooltipElement,
  };
}

export class DescriptionCitationElement extends CrLitElement {
  static get is() {
    return 'description-citation';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      url: {type: String},
      index: {type: Number},
      citationCount: {type: Number},
      productName: {type: String},
    };
  }

  url: string = '';
  index: number = 0;
  citationCount: number = 0;
  productName: string = '';

  protected openCitation_() {
    this.$.tooltip.hide();
    OpenWindowProxyImpl.getInstance().openUrl(this.url);
  }

  protected getAriaLabel_(): string {
    return loadTimeData.getStringF(
        'citationA11yLabel', this.index, this.citationCount, this.productName,
        getAbbreviatedUrl(this.url));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'description-citation': DescriptionCitationElement;
  }
}

customElements.define(
    DescriptionCitationElement.is, DescriptionCitationElement);
