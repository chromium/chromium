// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mwc/@material/web/focus/md-focus-ring.js';
import 'chrome://resources/cros_components/button/button.js';

import {
  classMap,
  createRef,
  css,
  CSSResultGroup,
  html,
  ifDefined,
  nothing,
  PropertyDeclarations,
  PropertyValues,
  Ref,
  ref,
  repeat,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {TextPart, Transcription} from '../core/soda/soda.js';
import {
  assert,
  assertExists,
  assertInstanceof,
} from '../core/utils/assert.js';
import {formatDuration} from '../core/utils/datetime.js';
import {clamp, parseNumber, sliceWhen} from '../core/utils/utils.js';

import {
  getNumSpeakerClass,
  getSpeakerLabelClass,
  SPEAKER_LABEL_COLORS,
} from './styles/speaker_label.js';

const SCROLL_MARGIN = 3;

function inBetween(x: number, [low, high]: [number, number]): boolean {
  // Note that .scrollTo sometimes scroll slightly off to what's given as an
  // argument, so we add a margin.
  return (
    x >= Math.min(low, high) - SCROLL_MARGIN &&
    x <= Math.max(low, high) + SCROLL_MARGIN
  );
}

export class TranscriptionView extends ReactiveLitElement {
  static override styles: CSSResultGroup = [
    SPEAKER_LABEL_COLORS,
    css`
      :host {
        display: block;
        position: relative;
      }

      #container {
        box-sizing: border-box;
        display: flex;
        flex-flow: column;
        gap: 12px;
        max-height: 100%;
        overflow-y: auto;
        padding: 12px 0 64px;
        width: 100%;
      }

      #transcript {
        display: grid;
        grid-template-columns:
          minmax(calc(12px + 40px + 10px), max-content)
          1fr;
      }

      .row {
        display: grid;
        grid-column: 1 / 3;
        grid-template-columns: subgrid;
        padding: 0 12px 0 0;
      }

      .timestamp {
        /*
         * Note that this need to be 0px instead of 0, since it's used in
         * calc().
         */
        --md-focus-ring-outward-offset: 0px;
        --md-focus-ring-shape: 4px;

        font: var(--cros-body-1-font);

        /*
         * Note that compared to the spec, 2px of left/right margin is moved to
         * padding so it's included in the hover / focus ring.
         */
        margin: 12px 8px 12px 10px;
        outline: none;
        padding: 0 2px;
        place-self: start;
        position: relative;

        .seekable & {
          cursor: pointer;
        }
      }

      .paragraph {
        font: var(--cros-body-1-font);
        padding: 12px;
      }

      .highlight-word {
        text-decoration: underline 1.5px;
        text-underline-offset: 3px;
      }

      .speaker-label {
        color: var(--speaker-label-shapes-color);
        font: var(--cros-button-1-font);
        margin: 0 0 4px;

        .speaker-single & {
          display: none;
        }
      }

      .speaker-pending {
        --speaker-label-shapes-color: var(--cros-sys-on_surface_variant);
      }

      .sentence {
        border-radius: 4px;
        box-decoration-break: clone;
        -webkit-box-decoration-break: clone;

        /* "Undo" the horizontal padding so the text aligns with the design. */
        margin: 0 -2px;

        /*
         * Note that while the font size is 13px, the background height without
         * padding would be 16px. Make it full line height (20px) by adding a
         * 2px vertical padding. (horizontal padding happens to also be 2px).
         */
        padding: 2px;

        .seekable & {
          cursor: pointer;

          &:hover,
          &:focus {
            background: var(--cros-sys-highlight_shape);
            outline: none;
          }
        }

        .seekable .timestamp:hover + .paragraph > &:first-of-type {
          background: var(--cros-sys-highlight_shape);
        }
      }

      #autoscroll-button {
        bottom: 16px;
        left: 0;
        margin: 0 auto;
        position: absolute;
        right: 0;

        /* TODO(pihsun): Transition between shown/hide state */
        #container.autoscroll + & {
          display: none;
        }
      }
    `,
  ];

  static override properties: PropertyDeclarations = {
    transcription: {attribute: false},
    currentTime: {type: Number},
    seekable: {type: Boolean},
  };

  transcription: Transcription|null = null;

  currentTime: number|null = null;

  seekable = false;

  autoscrollEnabled = signal(true);

  lastAutoScrollRange: [number, number]|null = null;

  lastAutoScrollTime: number|null = null;

  containerRef: Ref<HTMLElement> = createRef();

  // TODO(pihsun): Move all the autoscroll logic to a separate file /
  // ReactiveController.
  //
  // Autoscroll that is automatically stopped by user scroll is VERY hard
  // to get 100% correct, since there's no way to distinguish scroll events
  // that are originated from user input or autoscroll, especially when
  // we want smooth scroll which generates multiple scroll events.
  //
  // Some other issue that can make things complicate:
  // * Autoscroll can also be interrupted by user scroll, which doesn't
  //   generate a separate scrollend event.
  // * There's no good way of knowing if calling a .scrollTo will generate any
  //   scroll/scrollend event.
  //
  // Desired behavior of the heuristic:
  // * When there's no user input and either in recording or in playback, auto
  //   scroll must not stop by itself.
  // * User scroll should stop the autoscroll, but it's fine if occasionally
  //   "small" scroll that occurs at the same time of autoscroll got ignored.
  // * It's fine that other layout change originated from user (changing window
  //   width, ...) stops autoscroll.
  // * Clicking the autoscroll button should start autoscroll.
  //
  // The current "simple" heuristic:
  // * On each autoscroll, we remember the current scrollTop and the target
  //   scrollTop as the possible scroll event range due to autoscroll.
  // * On each scroll event:
  //   * If the container is at bottom of scroll, don't stop autoscroll, since
  //     the scrollTop might jump back due to the scrollHeight change.
  //   * If there's no current autoscroll range, or the scrollTop falls outside
  //     of the autoscroll range, stop autoscroll.
  //
  private onContainerScroll() {
    if (!this.autoscrollEnabled.value) {
      return;
    }

    const container = assertExists(this.containerRef.value);
    // When transcription is running, there's a chance that the transcription
    // will become shorter due to intermediate partialResults, which results
    // in the scrollTop being brought back. As a workaround, don't stop
    // autoscroll if the scroll is at near the bottom of the screen.
    if (container.scrollTop >=
        container.scrollHeight - container.offsetHeight - SCROLL_MARGIN) {
      return;
    }
    if (this.lastAutoScrollRange === null ||
        !inBetween(container.scrollTop, this.lastAutoScrollRange)) {
      this.autoscrollEnabled.value = false;
      this.lastAutoScrollRange = null;
    }
  }

  private onContainerScrollEnd() {
    this.lastAutoScrollRange = null;
  }

  override updated(changedProperties: PropertyValues<this>): void {
    if (!this.autoscrollEnabled.value) {
      return;
    }
    if (this.seekable &&
        (!changedProperties.has('currentTime') || this.currentTime === null)) {
      // Optimization: Don't rerun autoscroll to the highlighted word if
      // currentTime is not changed or is null.
      return;
    }
    this.runAutoScroll();
  }

  private runAutoScroll() {
    const now = Date.now();
    // TODO(pihsun): Ideally we want to skip scrolling when there's an
    // existing scrolling ongoing, but I can't managed to reliably get
    // that information since Chrome sometimes deliver some scroll event
    // without corresponding scrollend. Throttle the `scrollTo` instead
    // and only do a scrollTo at most once every 500ms, to ensure the
    // smooth scrolling can make some progress every time.
    if (this.lastAutoScrollTime !== null &&
        this.lastAutoScrollTime >= now - 500) {
      // Autoscroll just happened in the last 500ms.
      return;
    }
    const container = assertExists(this.containerRef.value);
    let targetScrollTop: number;
    if (this.seekable) {
      // TODO(pihsun): We might need "fake" highlight blocks between speech so
      // it'll scroll to the part between speech?
      const highlightedElement =
        this.shadowRoot?.querySelector('.highlight-word') ?? null;
      if (highlightedElement === null) {
        return;
      }
      // TODO(pihsun): Have a typed helper function for querySelector /
      // querySelectorAll with assertion for types.
      assert(highlightedElement instanceof HTMLElement);
      // We calculate the target scrollTop by ourselves instead of relying on
      // Element.scrollIntoView, so we can know the targetScrollTop for
      // autoscroll calculation.
      targetScrollTop = clamp(
        highlightedElement.offsetTop + highlightedElement.offsetHeight / 2 -
          container.clientHeight / 2,
        0,
        container.scrollHeight - container.offsetHeight,
      );
    } else {
      // Auto scroll to bottom.
      targetScrollTop = container.scrollHeight - container.offsetHeight;
    }
    // TODO(pihsun): scrollTo does nothing & don't call scrollend when the
    // target top is almost the same to the current scrollTop. Check Chrome
    // code to see what's the real condition on this.
    if (Math.abs(container.scrollTop - targetScrollTop) >= SCROLL_MARGIN) {
      this.lastAutoScrollRange = [container.scrollTop, targetScrollTop];
      this.lastAutoScrollTime = now;
      container.scrollTo({top: targetScrollTop, behavior: 'smooth'});
    }
  }

  private renderSentence(sentence: TextPart[]) {
    return repeat(
      sentence,
      (_v, i) => i,
      (part, i) => {
        const highlightWord = (() => {
          if (this.currentTime === null || part.timeRange === null) {
            return false;
          }
          return (
            this.currentTime >= part.timeRange.startMs / 1000 &&
            this.currentTime < part.timeRange.endMs / 1000
          );
        })();
        // For the first word, the leadingSpace is already added at the
        // sentence level. Otherwise we follows the leadingSpace for the part
        // and treat missing field as having a space.
        const leadingSpace = i === 0 ? false : part.leadingSpace ?? true;
        if (!highlightWord) {
          return `${leadingSpace ? ' ' : ''}${part.text}`;
        }
        return html`${leadingSpace ? ' ' : ''}<span class="highlight-word"
            >${part.text}</span
          >`;
      },
    );
  }

  private renderSpeakerLabel(
    speakerLabels: string[],
    speakerLabel: string|null,
    partial: boolean,
  ) {
    if (speakerLabel === null) {
      return nothing;
    }

    let speakerLabelClass: string;
    let speakerLabelLabel: string;

    if (partial) {
      speakerLabelClass = 'speaker-pending';
      speakerLabelLabel = i18n.transcriptionSpeakerLabelPendingLabel;
    } else {
      const speakerLabelIdx = speakerLabels.indexOf(speakerLabel);
      assert(speakerLabelIdx !== -1);
      speakerLabelClass = getSpeakerLabelClass(speakerLabelIdx);
      speakerLabelLabel = i18n.transcriptionSpeakerLabelLabel(speakerLabel);
    }

    return html`<div class="speaker-label ${speakerLabelClass}">
      ${speakerLabelLabel}
    </div>`;
  }

  private renderParagraphContent(parts: TextPart[]) {
    if (!this.seekable) {
      // Don't render each sentence/word as separate DOM node when there's no
      // need for seeking, so there would be fewer DOM nodes.
      return parts
        .map((part, i) => {
          const leadingSpace = part.leadingSpace ?? i > 0;
          return `${leadingSpace ? ' ' : ''}${part.text}`;
        })
        .join('');
    }
    // TODO: b/341014241 - Better heuristic for cutting sentences.
    const sentences = sliceWhen(parts, ({text}) => {
      return text.endsWith('.') || text.endsWith('?') || text.endsWith('!');
    });
    return repeat(
      sentences,
      (_v, i) => i,
      (sentence, i) => {
        // Use the leadingSpace field for the first word. If the
        // leadingSpace field is missing, add space after the first
        // sentence.
        const leadingSpace = sentence[0]?.leadingSpace ?? i > 0;
        return html`${leadingSpace ? ' ' : ''}<span
            class="sentence"
            data-start-ms=${ifDefined(sentence[0]?.timeRange?.startMs)}
            tabindex=${this.seekable ? 0 : -1}
            role="button"
            >${this.renderSentence(sentence)}</span
          >`;
      },
    );
  }

  private renderParagraph(speakerLabels: string[], parts: TextPart[]) {
    const {speakerLabel, partial} = assertExists(parts[0]);
    return [
      this.renderSpeakerLabel(speakerLabels, speakerLabel, partial ?? false),
      this.renderParagraphContent(parts),
    ];
  }

  private onTextClick(ev: MouseEvent) {
    const target = assertInstanceof(ev.target, HTMLElement);
    const parent = target.closest('[data-start-ms]');
    if (parent === null) {
      return;
    }
    const startMs = parseNumber(
      assertInstanceof(parent, HTMLElement).dataset['startMs'],
    );
    if (startMs === null) {
      return;
    }
    this.dispatchEvent(
      new CustomEvent('word-clicked', {detail: {startMs}}),
    );
    this.autoscrollEnabled.value = true;
  }

  private onAutoScrollButtonClick() {
    this.autoscrollEnabled.value = true;
    this.runAutoScroll();
  }

  override render(): RenderResult {
    if (this.transcription === null) {
      return nothing;
    }

    const speakerLabels = this.transcription.getSpeakerLabels();
    const paragraphs = this.transcription.getParagraphs();

    const content = repeat(
      paragraphs,
      (_parts, i) => i,
      (parts) => {
        const startTimeRange = assertExists(parts[0]).timeRange;
        const startTimeDisplay =
          startTimeRange === null ? '?' : formatDuration({
            milliseconds: startTimeRange.startMs,
          });
        // TODO(pihsun): Check if there's any case that timestamp will be
        // missing.
        // TODO(pihsun): Handle keyboard event / a11y on the timestamp.
        // TODO(pihsun): Check performance? Try to do CSS only highlight when
        // only currentTime are changed, so the whole template don't need to
        // be re-computed.
        return html`
          <div class="row">
            <span
              class="timestamp"
              tabindex=${this.seekable ? 0 : -1}
              data-start-ms=${ifDefined(startTimeRange?.startMs)}
              role="button"
            >
              ${startTimeDisplay}
              ${this.seekable ? html`<md-focus-ring></md-focus-ring>` : nothing}
            </span>
            <div class="paragraph">
              ${this.renderParagraph(speakerLabels, parts)}
            </div>
          </div>
        `;
      },
    );

    const classes = {
      seekable: this.seekable,
      autoscroll: this.autoscrollEnabled.value,
      [getNumSpeakerClass(speakerLabels.length)]: true,
    };
    // TODO(pihsun): @click on #transcript is a performance optimization to
    // only have the click handler on the container. Need to adjust this
    // accordingly when we have other clickable things inside the container
    // (speaker label).
    return html`<div
        id="container"
        class=${classMap(classes)}
        ${ref(this.containerRef)}
        @scroll=${this.onContainerScroll}
        @scrollend=${this.onContainerScrollEnd}
      >
        <slot></slot>
        <div
          id="transcript"
          @click=${this.seekable ? this.onTextClick : nothing}
        >
          ${content}
        </div>
      </div>
      <cros-button
        button-style="secondary"
        id="autoscroll-button"
        label=${i18n.transcriptionAutoscrollButton}
        @click=${this.onAutoScrollButtonClick}
      ></cros-button>`;
  }
}

window.customElements.define('transcription-view', TranscriptionView);

declare global {
  interface HTMLElementTagNameMap {
    'transcription-view': TranscriptionView;
  }
}
