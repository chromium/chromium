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

import {assert} from 'chrome://resources/js/assert.js';

import {isSeaPenTextInputEnabled} from './load_time_booleans.js';
import {MAXIMUM_SEARCH_WALLPAPER_TEXT_BYTES, SeaPenQuery, SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import {searchSeaPenThumbnails} from './sea_pen_controller.js';
import {getTemplate} from './sea_pen_input_query_element.html.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
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
      textValue_: String,

      thumbnails_: Object,

      thumbnailsLoading_: Boolean,

      maxTextLength_: {
        type: Number,
        value: Math.floor(MAXIMUM_SEARCH_WALLPAPER_TEXT_BYTES / 3),
      },
    };
  }

  private textValue_: string;
  private thumbnails_: SeaPenThumbnail[]|null;
  private thumbnailsLoading_: boolean;

  override connectedCallback() {
    assert(isSeaPenTextInputEnabled(), 'sea pen text input must be enabled');
    super.connectedCallback();
    this.watch<SeaPenInputQueryElement['thumbnails_']>(
        'thumbnails_', state => state.thumbnails);
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
  }

  private getSearchButtonText_(thumbnails: SeaPenThumbnail[]|null): string {
    return thumbnails ? this.i18n('seaPenRecreateButton') :
                        this.i18n('seaPenCreateButton');
  }

  private getSearchButtonIcon_(thumbnails: SeaPenThumbnail[]|null): string {
    return thumbnails ? 'personalization-shared:refresh' :
                        'sea-pen:photo-spark';
  }
}
customElements.define(SeaPenInputQueryElement.is, SeaPenInputQueryElement);
