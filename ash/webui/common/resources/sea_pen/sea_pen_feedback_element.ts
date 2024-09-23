// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the feedback buttons.
 */
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';

import {SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import {getTemplate} from './sea_pen_feedback_element.html.js';
import {WithSeaPenStore} from './sea_pen_store.js';

export enum FeedbackOption {
  UNSPECIFIED = 0,
  THUMBS_UP = 1,
  THUMBS_DOWN = 2,
}

export class SeaPenFeedbackElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-feedback';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedFeedbackOption: {
        type: String,
        value: FeedbackOption.UNSPECIFIED,
      },
      thumbnail: Object,
      inheritTabIndex: Number,
    };
  }

  selectedFeedbackOption: FeedbackOption;
  thumbnail: SeaPenThumbnail;
  inheritTabIndex: number;

  private notifySelectedOptionChanged_(isThumbsUp: boolean) {
    this.dispatchEvent(new CustomEvent('selected-feedback-changed', {
      bubbles: true,
      composed: true,
      detail: {isThumbsUp, thumbnailId: this.thumbnail.id},
    }));
  }

  private onClickThumbsUp_() {
    this.selectedFeedbackOption =
        this.selectedFeedbackOption === FeedbackOption.THUMBS_UP ?
        FeedbackOption.UNSPECIFIED :
        FeedbackOption.THUMBS_UP;
    if (this.selectedFeedbackOption === FeedbackOption.THUMBS_UP) {
      this.notifySelectedOptionChanged_(/*isThumbsUp=*/ true);
    }
  }

  private onClickThumbsDown_() {
    this.selectedFeedbackOption =
        this.selectedFeedbackOption === FeedbackOption.THUMBS_DOWN ?
        FeedbackOption.UNSPECIFIED :
        FeedbackOption.THUMBS_DOWN;
    if (this.selectedFeedbackOption === FeedbackOption.THUMBS_DOWN) {
      this.notifySelectedOptionChanged_(/*isThumbsUp=*/ false);
    }
  }

  private getThumbsUpIcon_(): string {
    return this.selectedFeedbackOption === FeedbackOption.THUMBS_UP ?
        'cr:thumbs-up-filled' :
        'cr:thumbs-up';
  }

  private getThumbsDownIcon_(): string {
    return this.selectedFeedbackOption === FeedbackOption.THUMBS_DOWN ?
        'cr:thumbs-down-filled' :
        'cr:thumbs-down';
  }
}

declare global {
  interface HTMLElementEventMap {
    'selected-feedback-changed': CustomEvent<{isThumbsUp: boolean}>;
  }

  interface HTMLElementTagNameMap {
    'sea-pen-feedback': SeaPenFeedbackElement;
  }
}

customElements.define(SeaPenFeedbackElement.is, SeaPenFeedbackElement);
