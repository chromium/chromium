// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the SeaPen templates and
 * result set of SeaPen wallpapers.
 */

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {WithPersonalizationStore} from '../../personalization_store.js';

import {getTemplate} from './sea_pen_collection_element.html.js';

/** Enumeration of supported tabs. */
export enum SeaPenTab {
  TEMPLATES = 'templates',
  IMAGE_RESULTS = 'image_results',
}

export interface SeaPenOption {
  // `value` is the actual option value to be sent to the server side.
  value: string;
  // `translation` is the translated value to be displayed in the UI.
  translation: string;
}

export interface SeaPenTemplate {
  preview: Url[];
  // `title` is the user-visible string in collection titles and breadcrumbs.
  title: string;
  // `text` is the string that shows up on the sea pen subpage.
  text: string;
  id: string;
  // `options` are in the form of 'option_name': [option1, option2, ...].
  options: Map<string, SeaPenOption[]>;
}

export class SeaPenCollectionElement extends WithPersonalizationStore {
  static get is() {
    return 'sea-pen-collection';
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
        type: SeaPenTab,
        value: SeaPenTab.TEMPLATES,
      },
    };
  }

  private templateId: string;
  private tab_: SeaPenTab;

  private onTemplateIdChanged_() {
    this.tab_ = this.templateId ? SeaPenTab.IMAGE_RESULTS : SeaPenTab.TEMPLATES;
  }

  private shouldShowTemplates_(): boolean {
    return this.tab_ === SeaPenTab.TEMPLATES;
  }

  private shouldShowImages_(): boolean {
    return this.tab_ == SeaPenTab.IMAGE_RESULTS;
  }
}

customElements.define(SeaPenCollectionElement.is, SeaPenCollectionElement);
