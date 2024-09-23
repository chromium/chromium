// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-image.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';

import {css, CSSResultGroup, html} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {GenaiResultType} from '../core/on_device_model/types.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {assertExhaustive} from '../core/utils/assert.js';

export enum UserRating {
  THUMB_UP,
  THUMB_DOWN,
}

export class GenaiFeedbackButtons extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    :host {
      background-color: var(--background-color);
      border-radius: 18px 0 0;
      display: flex;
      flex-flow: row;
      gap: 8px;
      padding: 4px 4px 0 8px;

      & > cra-icon-button {
        margin: 0;
      }

      & > svg {
        color: var(--background-color);
        position: absolute;
        z-index: -1;
      }

      & > .top-right {
        right: 0;
        top: -10px;
      }

      & > .bottom-left {
        bottom: 0;
        left: -10px;
      }
    }
  `;

  // TODO(pihsun): Reset rating on "output" change.
  private readonly userRating = signal<UserRating|null>(null);

  private readonly platformHandler = usePlatformHandler();

  resultType: GenaiResultType|null = null;

  private sendFeedbackEvent(rating: UserRating): void {
    if (this.resultType === null) {
      return;
    }

    const isPositive = rating === UserRating.THUMB_UP;
    const eventsSender = this.platformHandler.eventsSender;

    switch (this.resultType) {
      case GenaiResultType.TITLE_SUGGESTION:
        return eventsSender.sendFeedbackTitleSuggestionEvent({isPositive});
      case GenaiResultType.SUMMARY:
        return eventsSender.sendFeedbackSummaryEvent({isPositive});
      default:
        assertExhaustive(this.resultType);
    }
  }

  private onThumbUpClick() {
    if (this.userRating.value === UserRating.THUMB_UP) {
      this.userRating.value = null;
    } else {
      this.userRating.value = UserRating.THUMB_UP;
      this.sendFeedbackEvent(UserRating.THUMB_UP);
    }
  }

  private onThumbDownClick() {
    if (this.userRating.value === UserRating.THUMB_DOWN) {
      this.userRating.value = null;
    } else {
      this.userRating.value = UserRating.THUMB_DOWN;
      this.sendFeedbackEvent(UserRating.THUMB_DOWN);
      // TODO: b/344789836 - Determine what should be the default description
      // for the feedback report (we likely want the model input & output), and
      // also put it into recorder_strings.grdp for i18n.
      this.platformHandler.showAiFeedbackDialog('#RecorderApp');
    }
  }

  override render(): RenderResult {
    const rating = this.userRating.value;
    return html`
      <!-- These are the two additional "rounded corner". -->
      <svg class="top-right" width="10" height="10">
        <path d="M 10 10 H 0 a 10 10 0 0 0 10 -10 V 10" fill="currentcolor" />
      </svg>
      <svg class="bottom-left" width="10" height="10" part="bottom-left-corner">
        <path d="M 10 10 H 0 a 10 10 0 0 0 10 -10 V 10" fill="currentcolor" />
      </svg>
      <cra-icon-button
        buttonstyle="toggle"
        size="small"
        .selected=${rating === UserRating.THUMB_UP}
        @click=${this.onThumbUpClick}
        aria-label=${i18n.genaiPositiveFeedbackButtonTooltip}
      >
        <cra-icon name="thumb_up" slot="icon"></cra-icon>
        <cra-icon name="thumb_up_filled" slot="selectedIcon"></cra-icon>
      </cra-icon-button>
      <cra-icon-button
        buttonstyle="toggle"
        size="small"
        .selected=${rating === UserRating.THUMB_DOWN}
        @click=${this.onThumbDownClick}
        aria-label=${i18n.genaiNegativeFeedbackButtonTooltip}
      >
        <cra-icon name="thumb_down" slot="icon"></cra-icon>
        <cra-icon name="thumb_down_filled" slot="selectedIcon"></cra-icon>
      </cra-icon-button>
    `;
  }
}

window.customElements.define('genai-feedback-buttons', GenaiFeedbackButtons);

declare global {
  interface HTMLElementTagNameMap {
    'genai-feedback-buttons': GenaiFeedbackButtons;
  }
}
