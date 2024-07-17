// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/card/card.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';

import {
  css,
  html,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';
import {RecordingMetadata} from '../core/recording_data_manager.js';
import {assertExists, assertInstanceof} from '../core/utils/assert.js';
import {
  formatDate,
  formatDuration,
  formatTime,
} from '../core/utils/datetime.js';
import {stopPropagation} from '../core/utils/event_handler.js';

/**
 * An item in the recording list.
 */
export class RecordingFileListItem extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    .recording {
      --cros-card-padding: 24px;
      --cros-card-hover-color: none;

      position: relative;

      & > cros-card {
        background-color: var(--cros-sys-app_base);
        min-height: initial;
        width: initial;
        -webkit-tap-highlight-color: transparent;

        &::part(content) {
          align-items: start;
          cursor: pointer;
          display: flex;
          flex-flow: row;
          gap: 16px;
        }
      }

      & > .options {
        position: absolute;
        right: 0;
        top: 0;
        z-index: 1;
      }
    }

    .recording-info {
      align-items: stretch;
      display: flex;
      flex: 1;
      flex-flow: column;
      gap: 8px;
      min-width: 0;
    }

    .title {
      font: var(--cros-title-1-font);
      overflow: hidden;

      /* To avoid overlap with the options button. */
      padding-inline-end: 36px;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .timeline {
      background-color: var(--cros-sys-primary);
      border-radius: 2px;
      height: 4px;
      margin-top: 16px;
    }

    .timestamps {
      display: flex;
      flex-flow: row;
      font: var(--cros-body-2-font);
      gap: 24px;

      & > span:first-child {
        flex: 1;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    recording: {attribute: false},
  };

  recording: RecordingMetadata|null = null;

  private onRecordingClick(ev: PointerEvent) {
    const target = assertInstanceof(ev.currentTarget, HTMLElement);
    const id = assertExists(target.dataset['recordingId']);
    this.dispatchEvent(
      new CustomEvent('recording-clicked', {
        detail: id,
        bubbles: true,
        composed: true,
      }),
    );
  }

  private onPlayClick(ev: PointerEvent) {
    // TODO: b/336963138 - Implements inline playing.
    ev.preventDefault();
    ev.stopPropagation();
  }

  private onOptionsClick(ev: PointerEvent) {
    // TODO: b/336963138 - Implements options.
    ev.preventDefault();
    ev.stopPropagation();
  }

  private renderRecordingTimeline(recording: RecordingMetadata) {
    const recordingDurationDisplay = formatDuration({
      milliseconds: recording.durationMs,
    });
    // TODO: b/336963138 - Actually render which parts have speech.
    return [
      html`<div class="timeline"></div>`,
      html`<div class="timestamps">
        <span>
          ${formatDate(recording.recordedAt)} •
          ${formatTime(recording.recordedAt)}
        </span>
        <span>${recordingDurationDisplay}</span>
      </div>`,
    ];
  }

  override render(): RenderResult {
    if (this.recording === null) {
      return nothing;
    }
    // TODO(pihsun): Check why the ripple sometimes doesn't happen on touch
    // long-press but sometimes does.
    return html`<div class="recording">
      <cros-card
        @click=${this.onRecordingClick}
        data-recording-id=${this.recording.id}
        cardstyle="filled"
        tabindex="0"
        interactive
      >
        <cra-icon-button
          shape="circle"
          @click=${this.onPlayClick}
          @pointerdown=${/* To prevent ripple on card. */ stopPropagation}
        >
          <cra-icon slot="icon" name="play_arrow"></cra-icon>
        </cra-icon-button>
        <div class="recording-info">
          <div class="title">${this.recording.title}</div>
          ${this.renderRecordingTimeline(this.recording)}
        </div>
      </cros-card>
      <cra-icon-button
        buttonstyle="floating"
        class="options"
        @click=${this.onOptionsClick}
      >
        <cra-icon slot="icon" name="more_vertical"></cra-icon>
      </cra-icon-button>
    </div>`;
  }
}

window.customElements.define('recording-file-list-item', RecordingFileListItem);

declare global {
  interface HTMLElementTagNameMap {
    'recording-file-list-item': RecordingFileListItem;
  }
}
