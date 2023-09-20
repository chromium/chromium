// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays input for SeaPen wallpaper.
 */

import {WithPersonalizationStore} from '../../personalization_store.js';
import {QUERY} from '../utils.js';

import {getTemplate} from './sea_pen_input_element.html.js';

/** Enumeration of supported tabs. */
export enum SeaPenQueryTab {
  INPUT_QUERY = 'input_query',
  TEMPLATE_QUERY = 'template_query',
}

export class SeaPenInputElement extends WithPersonalizationStore {
  static get is() {
    return 'sea-pen-input';
  }
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      templateId: {
        type: String,
        observer: 'onTemplateIdChanged_',
      },

      tab_: {
        type: SeaPenQueryTab,
        value: SeaPenQueryTab.INPUT_QUERY,
      },
    };
  }

  private templateId: string|null;
  private tab_: SeaPenQueryTab;

  private onTemplateIdChanged_(templateId: string|null) {
    this.tab_ = templateId && templateId != QUERY ?
        SeaPenQueryTab.TEMPLATE_QUERY :
        SeaPenQueryTab.INPUT_QUERY;
  }

  private shouldShowInputQuery_(tab: SeaPenQueryTab): boolean {
    return tab === SeaPenQueryTab.INPUT_QUERY;
  }

  private shouldShowTemplateQuery_(tab: SeaPenQueryTab) {
    return tab === SeaPenQueryTab.TEMPLATE_QUERY;
  }
}
customElements.define(SeaPenInputElement.is, SeaPenInputElement);
