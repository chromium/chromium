// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays user input to search for
 * SeaPen wallpapers.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import '../../../common/icons.html.js';
import '../../../css/wallpaper.css.js';
import '../../../css/cros_button_style.css.js';

import {assert} from 'chrome://resources/js/assert.js';

import {MAXIMUM_SEARCH_WALLPAPER_TEXT_BYTES, SeaPenQuery} from '../../../sea_pen.mojom-webui.js';

import {QUERY} from './constants.js';
import {isSeaPenTextInputEnabled} from './load_time_booleans.js';
import {searchSeaPenThumbnails} from './sea_pen_controller.js';
import {getTemplate} from './sea_pen_input_query_element.html.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {SeaPenPaths, SeaPenRouterElement} from './sea_pen_router_element.js';
import {WithSeaPenStore} from './sea_pen_store.js';

export class SeaPenInputQueryElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-input-query';
  }
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path: String,

      textValue_: String,

      thumbnailsLoading_: Boolean,

      maxTextLength_: {
        type: Number,
        value: Math.floor(MAXIMUM_SEARCH_WALLPAPER_TEXT_BYTES / 3),
      },
    };
  }

  private textValue_: string;
  private thumbnailsLoading_: boolean;
  path: string;

  override connectedCallback() {
    assert(isSeaPenTextInputEnabled(), 'sea pen text input must be enabled');
    super.connectedCallback();
    this.watch<SeaPenInputQueryElement['thumbnailsLoading_']>(
        'thumbnailsLoading_', state => state.loading.thumbnails);
    this.updateFromStore();
  }

  private onClickInputQuerySearchButton_() {
    assert(this.textValue_, 'input query should not be empty.');
    const query: SeaPenQuery = {
      textQuery: this.textValue_,
    };
    searchSeaPenThumbnails(query, getSeaPenProvider(), this.getStore());
    SeaPenRouterElement.instance().goToRoute(
        SeaPenPaths.RESULTS, {seaPenTemplateId: QUERY});
  }

  private getSearchButtonText_(path: string|null): string {
    // TODO(b/308200616) Add finalized text.
    switch (path) {
      case SeaPenPaths.RESULTS:
        return 'Search again';
      case SeaPenPaths.ROOT:
      default:
        return 'Search';
    }
  }

  private getSearchButtonIcon_(path: string|null): string {
    switch (path) {
      case SeaPenPaths.RESULTS:
        return 'personalization:refresh';
      case SeaPenPaths.ROOT:
      default:
        return 'sea-pen:photo-spark';
    }
  }
}
customElements.define(SeaPenInputQueryElement.is, SeaPenInputQueryElement);
