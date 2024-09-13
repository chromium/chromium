// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/card/card.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';

import {Card} from 'chrome://resources/cros_components/card/card.js';
import {
  classMap,
  createRef,
  css,
  html,
  map,
  nothing,
  PropertyDeclarations,
  ref,
  styleMap,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {
  RecordingMetadata,
  TimelineSegmentKind,
} from '../core/recording_data_manager.js';
import {assertExhaustive, assertExists} from '../core/utils/assert.js';
import {
  formatDate,
  formatDuration,
  formatTime,
} from '../core/utils/datetime.js';
import {stopPropagation} from '../core/utils/event_handler.js';
import {clamp} from '../core/utils/utils.js';

import {CraIconButton} from './cra/cra-icon-button.js';
import {
  getNumSpeakerClass,
  SPEAKER_LABEL_COLORS,
} from './styles/speaker_label.js';

/**
 * An item in the recording list.
 */
export class RecordingFileListItem extends ReactiveLitElement {
  static override styles = [
    SPEAKER_LABEL_COLORS,
    css`
      :host {
        display: block;
      }

      #root {
        position: relative;
      }

      #recording {
        --cros-card-padding: 24px;
        --cros-card-hover-color: none;

        margin: 0 32px;
        position: relative;

        /* TODO: b/336963138 - Align with the motion spec. */
        transition: transform 200ms ease;
        z-index: 1;

        &.menu-shown {
          transform: translateX(-144px);
        }

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

          & > .play-button.has-progress {
            position: relative;

            &::before {
              background: conic-gradient(
                var(--cros-sys-primary) calc(1% * var(--progress)),
                var(--cros-sys-primary_container) calc(1% * var(--progress))
              );
              border-radius: 50%;
              content: "";
              inset: -4px;
              position: absolute;
              z-index: -1;
            }
          }
        }
      }

      #recording-info {
        align-items: stretch;
        display: flex;
        flex: 1;
        flex-flow: column;
        min-width: 0;
      }

      #title {
        font: var(--cros-title-1-font);
        overflow: hidden;

        /* To avoid overlap with the options button. */
        padding-inline-end: 36px;
        text-overflow: ellipsis;
        white-space: nowrap;

        & > .highlight {
          background-color: var(--cros-sys-highlight_text);
        }
      }

      #description {
        -webkit-box-orient: vertical;
        color: var(--cros-sys-on_surface_variant);
        display: -webkit-box;
        font: var(--cros-body-1-font);
        -webkit-line-clamp: 2;
        margin-top: 4px;
        overflow: hidden;
        text-overflow: ellipsis;
      }

      #timeline {
        border-radius: 2px;
        display: flex;
        flex-flow: row;
        height: 4px;
        margin-top: 16px;
        overflow: hidden;

        & > .full {
          flex: 1;
        }

        & > div {
          background: var(--speaker-label-shapes-color);
        }

        &.speaker-single > .speaker-1 {
          background: var(--cros-sys-primary);
        }

        & > .no-audio {
          background: var(--cros-sys-primary_container);
        }

        & > .audio {
          background: var(--cros-sys-inverse_primary);
        }
      }

      #timestamps {
        display: flex;
        flex-flow: row;
        font: var(--cros-body-2-font);
        gap: 24px;
        margin-top: 8px;

        & > span:first-child {
          flex: 1;
        }
      }

      #options {
        position: absolute;
        right: 0;
        top: 0;
        z-index: 1;
      }

      #menu {
        align-items: center;
        bottom: 0;
        display: flex;
        flex-flow: row;
        margin: auto 0;
        padding: 0;
        position: absolute;
        right: 32px;
        top: 0;
        transition: right 200ms ease;

        &.menu-shown {
          right: 16px;
        }
      }
    `,
  ];

  static override properties: PropertyDeclarations = {
    recording: {attribute: false},
    searchHighlight: {attribute: false},
    playing: {type: Boolean},
    playProgress: {type: Number},
  };

  recording: RecordingMetadata|null = null;

  searchHighlight: [number, number]|null = null;

  /**
   * Whether the inline playing of this item is ongoing.
   */
  playing = false;

  /**
   * The progress of the inline playing, should be a number between [0, 100].
   */
  playProgress: number|null = null;

  private readonly menuShown = signal(false);

  private readonly platformHandler = usePlatformHandler();

  private readonly recordingCard = createRef<Card>();

  private readonly optionsButtonRef = createRef<CraIconButton>();

  get recordingCardForTest(): Card {
    return assertExists(this.recordingCard.value);
  }

  focusOnOption(): void {
    this.optionsButtonRef.value?.focus();
  }

  private onRecordingClick() {
    if (this.recording === null) {
      return;
    }

    this.dispatchEvent(
      new CustomEvent('recording-clicked', {
        detail: this.recording.id,
        bubbles: true,
        composed: true,
      }),
    );
  }

  private onDeleteRecordingClick() {
    if (this.recording === null) {
      return;
    }

    this.dispatchEvent(
      new CustomEvent('delete-recording-clicked', {
        detail: this.recording.id,
        bubbles: true,
        composed: true,
      }),
    );
  }

  private onExportRecordingClick() {
    if (this.recording === null) {
      return;
    }

    this.dispatchEvent(
      new CustomEvent('export-recording-clicked', {
        detail: this.recording.id,
        bubbles: true,
        composed: true,
      }),
    );
  }

  private onShowRecordingInfoClick() {
    if (this.recording === null) {
      return;
    }

    this.dispatchEvent(
      new CustomEvent('show-recording-info-clicked', {
        detail: this.recording.id,
        bubbles: true,
        composed: true,
      }),
    );
  }

  private onPlayClick(ev: PointerEvent) {
    // TODO: b/336963138 - Implements inline playing.
    ev.preventDefault();
    ev.stopPropagation();

    if (this.recording === null) {
      return;
    }

    this.dispatchEvent(
      new CustomEvent('play-recording-clicked', {
        detail: this.recording.id,
        bubbles: true,
        composed: true,
      }),
    );
  }

  private onOptionsClick() {
    this.menuShown.update((s) => !s);
  }

  private onFocusOut(ev: FocusEvent) {
    const target = ev.relatedTarget;
    if (target !== null && target instanceof Node &&
        this.shadowRoot?.contains(target)) {
      // New target still within this element.
      return;
    }
    this.menuShown.value = false;
  }

  private renderTitle(title: string, highlight: [number, number]|null) {
    if (highlight === null) {
      return html`<div id="title">${title}</div>`;
    }
    const [start, end] = highlight;
    return html`<div id="title">
      ${title.slice(0, start)}<span class="highlight"
        >${title.slice(start, end)}</span
      >${title.slice(end)}
    </div>`;
  }

  private renderDescription(description: string) {
    if (description.length === 0) {
      return nothing;
    }
    return html`<div id="description">${description}</div>`;
  }

  private renderRecordingTimelineColors(recording: RecordingMetadata) {
    if (recording.timelineSegments === undefined) {
      // The timeline segments is still being recalculated from the old
      // recordings. Show a timeline with full "audio".
      return html`<div class="audio full"></div>`;
    }
    function toClass(kind: TimelineSegmentKind) {
      switch (kind) {
        case TimelineSegmentKind.NO_AUDIO:
          return 'no-audio';
        case TimelineSegmentKind.AUDIO:
          return 'audio';
        case TimelineSegmentKind.SPEECH:
        case TimelineSegmentKind.SPEECH_SPEAKER_COLOR_1:
          return 'speaker-1';
        case TimelineSegmentKind.SPEECH_SPEAKER_COLOR_2:
          return 'speaker-2';
        case TimelineSegmentKind.SPEECH_SPEAKER_COLOR_3:
          return 'speaker-3';
        case TimelineSegmentKind.SPEECH_SPEAKER_COLOR_4:
          return 'speaker-4';
        case TimelineSegmentKind.SPEECH_SPEAKER_COLOR_5:
          return 'speaker-5';
        default:
          assertExhaustive(kind);
      }
    }
    const {segments} = recording.timelineSegments;
    return map(segments, ([length, kind]) => {
      const style = {flex: length};
      return html`<div class=${toClass(kind)} style=${styleMap(style)}></div>`;
    });
  }

  private renderRecordingTimeline(recording: RecordingMetadata) {
    const recordingDurationDisplay = formatDuration({
      milliseconds: recording.durationMs,
    });
    // Transcription off colors are compatible with colors when there's a single
    // speaker.
    const numSpeakerClass = getNumSpeakerClass(recording.numSpeakers ?? 1);
    return [
      html`<div id="timeline" class=${numSpeakerClass}>
        ${this.renderRecordingTimelineColors(recording)}
      </div>`,
      html`<div id="timestamps">
        <span>
          ${formatDate(this.platformHandler.getLocale(), recording.recordedAt)}
          •
          ${formatTime(this.platformHandler.getLocale(), recording.recordedAt)}
        </span>
        <span>${recordingDurationDisplay}</span>
      </div>`,
    ];
  }

  private renderPlayButton() {
    const playIcon = this.playing ? 'pause' : 'play_arrow';
    const classes = {
      'has-progress': this.playProgress !== null,
    };
    const styles = {
      '--progress':
        this.playProgress === null ? null : clamp(this.playProgress, 0, 100),
    };
    const title = assertExists(this.recording?.title);
    const ariaLabel = this.playing ?
      i18n.recordingItemPauseButtonAriaLabel(title) :
      i18n.recordingItemPlayButtonAriaLabel(title);

    return html`
      <cra-icon-button
        class="play-button ${classMap(classes)}"
        style=${styleMap(styles)}
        shape="circle"
        @click=${this.onPlayClick}
        @pointerdown=${/* To prevent ripple on card. */ stopPropagation}
        aria-label=${ariaLabel}
      >
        <cra-icon slot="icon" .name=${playIcon}></cra-icon>
      </cra-icon-button>
    `;
  }

  override render(): RenderResult {
    if (this.recording === null) {
      return nothing;
    }

    const classes = {
      'menu-shown': this.menuShown.value,
    };
    const title = this.recording.title;

    // TODO(pihsun): Check why the ripple sometimes doesn't happen on touch
    // long-press but sometimes does.
    // TODO: b/336963138 - Implements swipe left/right on the card to open/close
    // menu.
    return html`
      <div id="root" @focusout=${this.onFocusOut}>
        <div id="recording" class=${classMap(classes)}>
          <cros-card
            @click=${this.onRecordingClick}
            cardstyle="filled"
            tabindex="0"
            interactive
            ${ref(this.recordingCard)}
          >
            ${this.renderPlayButton()}
            <div id="recording-info">
              ${this.renderTitle(title, this.searchHighlight)}
              ${this.renderDescription(this.recording.description)}
              ${this.renderRecordingTimeline(this.recording)}
            </div>
          </cros-card>
          <cra-icon-button
            buttonstyle="floating"
            id="options"
            @click=${this.onOptionsClick}
            aria-label=${i18n.recordingItemOptionsButtonAriaLabel(title)}
            aria-expanded=${this.menuShown.value}
            ${ref(this.optionsButtonRef)}
          >
            <cra-icon slot="icon" name="more_vertical"></cra-icon>
          </cra-icon-button>
        </div>
        <div id="menu" class=${classMap(classes)}>
          <cra-icon-button
            buttonstyle="floating"
            ?disabled=${!this.menuShown.value}
            aria-hidden=${!this.menuShown.value}
            @click=${this.onShowRecordingInfoClick}
            aria-label=${i18n.playbackMenuShowDetailOption}
          >
            <cra-icon slot="icon" name="info"></cra-icon>
          </cra-icon-button>
          <cra-icon-button
            buttonstyle="floating"
            ?disabled=${!this.menuShown.value}
            aria-hidden=${!this.menuShown.value}
            @click=${this.onExportRecordingClick}
            aria-label=${i18n.playbackMenuExportOption}
          >
            <cra-icon slot="icon" name="export"></cra-icon>
          </cra-icon-button>
          <cra-icon-button
            buttonstyle="floating"
            ?disabled=${!this.menuShown.value}
            aria-hidden=${!this.menuShown.value}
            @click=${this.onDeleteRecordingClick}
            aria-label=${i18n.playbackMenuDeleteOption}
          >
            <cra-icon slot="icon" name="delete"></cra-icon>
          </cra-icon-button>
        </div>
      </div>
    `;
  }
}

window.customElements.define('recording-file-list-item', RecordingFileListItem);

declare global {
  interface HTMLElementTagNameMap {
    'recording-file-list-item': RecordingFileListItem;
  }
}
