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
import 'chrome://resources/ash/common/sea_pen/sea_pen.css.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_icons.html.js';
import 'chrome://resources/ash/common/sea_pen/sea_pen_suggestions_element.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';

import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {beforeNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {QUERY} from './constants.js';
import {isSeaPenTextInputEnabled} from './load_time_booleans.js';
import {MAXIMUM_GET_SEA_PEN_THUMBNAILS_TEXT_BYTES, SeaPenQuery, SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import {getSeaPenThumbnails} from './sea_pen_controller.js';
import {getTemplate} from './sea_pen_input_query_element.html.js';
import {getSeaPenProvider} from './sea_pen_interface_provider.js';
import {logGenerateSeaPenWallpaper, logNumWordsInTextQuery} from './sea_pen_metrics_logger.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {SeaPenSuggestionSelectedEvent} from './sea_pen_suggestions_element.js';
import {isSelectionEvent} from './sea_pen_utils.js';

export interface SeaPenInputQueryElement {
  $: {
    container: HTMLDivElement,
    queryInput: CrInputElement,
    searchButton: HTMLElement,
  };
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
      textValue_: {
        type: String,
        observer: 'onTextValueUpdated_',
      },

      seaPenQuery_: {
        type: Object,
        value: null,
        observer: 'onSeaPenQueryChanged_',
      },

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
        value: Math.floor(MAXIMUM_GET_SEA_PEN_THUMBNAILS_TEXT_BYTES / 3),
      },

      shouldShowSuggestions_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private textValue_: string;
  private seaPenQuery_: SeaPenQuery|null;
  private thumbnails_: SeaPenThumbnail[]|null;
  private thumbnailsLoading_: boolean;
  private searchButtonText_: string|null;
  private searchButtonIcon_: string;
  private shouldShowSuggestions_: boolean;
  private containerOriginalHeight_: number;
  private resizeObserver_: ResizeObserver;

  override connectedCallback() {
    assert(isSeaPenTextInputEnabled(), 'sea pen text input must be enabled');
    super.connectedCallback();
    this.watch<SeaPenInputQueryElement['thumbnails_']>(
        'thumbnails_', state => state.thumbnails);
    this.watch<SeaPenInputQueryElement['thumbnailsLoading_']>(
        'thumbnailsLoading_', state => state.loading.thumbnails);
    this.watch<SeaPenInputQueryElement['seaPenQuery_']>(
        'seaPenQuery_', state => state.currentSeaPenQuery);
    this.updateFromStore();

    this.$.queryInput.focusInput();

    this.resizeObserver_ =
        new ResizeObserver(() => this.animateContainerHeight());

    beforeNextRender(this, () => {
      this.containerOriginalHeight_ = this.$.container.scrollHeight;
      this.$.container.style.height = `${this.containerOriginalHeight_}px`;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.resizeObserver_.disconnect();
  }

  // Called when there is a custom dom-change event dispatched from
  // `sea-pen-suggestions` element.
  private onSeaPenSuggestionsDomChanged_() {
    const suggestionsContainer =
        this.shadowRoot!.querySelector('sea-pen-suggestions');
    if (suggestionsContainer) {
      this.resizeObserver_.observe(suggestionsContainer);
    }
  }

  // Updates main container's height and applies transition style.
  private animateContainerHeight() {
    const suggestionsContainer =
        this.shadowRoot!.querySelector('sea-pen-suggestions');
    const suggestionsContainerHeight =
        suggestionsContainer ? suggestionsContainer.scrollHeight : 0;
    this.$.container.style.height =
        `${this.containerOriginalHeight_ + suggestionsContainerHeight}px`;
  }

  private onSeaPenQueryChanged_(seaPenQuery: SeaPenQuery|null) {
    if (!seaPenQuery || !seaPenQuery.textQuery) {
      return;
    }
    this.textValue_ = seaPenQuery.textQuery;
  }

  private onClickInputQuerySearchButton_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }
    assert(this.textValue_, 'input query should not be empty.');
    // This only works for English. We only support English queries for now.
    logNumWordsInTextQuery(this.textValue_.split(/\s+/).length);
    const query: SeaPenQuery = {
      textQuery: this.textValue_,
    };
    getSeaPenThumbnails(query, getSeaPenProvider(), this.getStore());
    logGenerateSeaPenWallpaper(QUERY);
    // Stop the event propagation, otherwise, the event will be passed to parent
    // element, this.onClick_ will be triggered improperly.
    event.preventDefault();
    event.stopPropagation();

    // Hide suggestions when creating thumbnails.
    this.shouldShowSuggestions_ = false;
  }

  private onSuggestionSelected_(event: SeaPenSuggestionSelectedEvent) {
    this.textValue_ = this.textValue_.trim();
    this.textValue_ = this.textValue_.length > 0 ?
        `${this.textValue_}, ${event.detail}` :
        event.detail;
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

  private onTextValueUpdated_() {
    // Show suggestions when there is text input.
    this.shouldShowSuggestions_ = !!this.textValue_;
  }

  private onFocusChanged_() {
    // Show suggestions if there are thumbnails and text  input.
    this.shouldShowSuggestions_ = !!this.thumbnails_ && !!this.textValue_;
  }
}
customElements.define(SeaPenInputQueryElement.is, SeaPenInputQueryElement);
