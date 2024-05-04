// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays user input to search for
 * SeaPen wallpapers.
 */

import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/personalization/personalization_shared_icons.html.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';

import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {isSeaPenTextInputEnabled} from './load_time_booleans.js';
import {MAXIMUM_SEARCH_WALLPAPER_TEXT_BYTES, SeaPenQuery, SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import {searchSeaPenThumbnails} from './sea_pen_controller.js';
import {getTemplate} from './sea_pen_input_query_element.html.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {WithSeaPenStore} from './sea_pen_store.js';

export interface SeaPenInputQueryElement {
  $: {queryInput: CrInputElement};
}

export class SeaPenInputQueryElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-input-query';
  }
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      textValue_: String,

      thumbnails_: {
        type: Object,
        observer: 'updateSearchButton_',
      },

      thumbnailsLoading_: Boolean,

      searchButtonText_: {
        type: String,
        value() {
          return loadTimeData.getString('seaPenCreateButton');
        },
      },

      searchButtonIcon_: {
        type: String,
        value() {
          return 'sea-pen:photo-spark';
        },
      },

      maxTextLength_: {
        type: Number,
        value: Math.floor(MAXIMUM_SEARCH_WALLPAPER_TEXT_BYTES / 3),
      },
    };
  }

  private textValue_: string;
  private thumbnails_: SeaPenThumbnail[]|null;
  private thumbnailsLoading_: boolean;
  private searchButtonText_: string|null;
  private searchButtonIcon_: string;

  override connectedCallback() {
    assert(isSeaPenTextInputEnabled(), 'sea pen text input must be enabled');
    super.connectedCallback();
    this.watch<SeaPenInputQueryElement['thumbnails_']>(
        'thumbnails_', state => state.thumbnails);
    this.watch<SeaPenInputQueryElement['thumbnailsLoading_']>(
        'thumbnailsLoading_', state => state.loading.thumbnails);
    this.updateFromStore();

    this.$.queryInput.focusInput();
  }

  private onClickInputQuerySearchButton_(event: Event) {
    assert(this.textValue_, 'input query should not be empty.');
    const query: SeaPenQuery = {
      textQuery: this.textValue_,
    };
    searchSeaPenThumbnails(query, getSeaPenProvider(), this.getStore());
    // Stop the event propagation, otherwise, the event will be passed to parent
    // element, this.onClick_ will be triggered improperly.
    event.preventDefault();
    event.stopPropagation();
  }

  private updateSearchButton_(thumbnails: SeaPenThumbnail[]|null) {
    if (!thumbnails) {
      // The thumbnails are not loaded yet.
      this.searchButtonText_ = this.i18n('seaPenCreateButton');
      this.searchButtonIcon_ = 'sea-pen:photo-spark';
    } else {
      this.searchButtonText_ = this.i18n('seaPenRecreateButton');
      this.searchButtonIcon_ = 'personalization-shared:refresh';
    }
  }
}
customElements.define(SeaPenInputQueryElement.is, SeaPenInputQueryElement);
