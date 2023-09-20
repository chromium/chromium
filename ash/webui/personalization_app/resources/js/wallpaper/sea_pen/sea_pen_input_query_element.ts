// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays user input to search for
 * SeaPen wallpapers.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {PersonalizationRouterElement} from '../../personalization_router_element.js';
import {WithPersonalizationStore} from '../../personalization_store.js';
import {QUERY} from '../utils.js';
import {searchImageThumbnails} from '../wallpaper_controller.js';

import {getTemplate} from './sea_pen_input_query_element.html.js';

export class SeaPenInputQueryElement extends WithPersonalizationStore {
  static get is() {
    return 'sea-pen-input-query';
  }
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      textValue_: String,

      query_: String,

      thumbnailsLoading_: Boolean,

    };
  }

  private textValue_: string;
  private query_: string|null;
  private thumbnailsLoading_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenInputQueryElement['query_']>(
        'query_', state => state.wallpaper.seaPen.query);
    this.watch<SeaPenInputQueryElement['thumbnailsLoading_']>(
        'thumbnailsLoading_',
        state => state.wallpaper.seaPen.thumbnailsLoading);
    this.updateFromStore();
  }

  private onClickInputQuerySearchButton_() {
    assert(this.textValue_, 'input query should not be empty.');
    searchImageThumbnails(this.textValue_, this.getStore());
    PersonalizationRouterElement.instance().selectSeaPenTemplate(QUERY);
  }
}
customElements.define(SeaPenInputQueryElement.is, SeaPenInputQueryElement);
