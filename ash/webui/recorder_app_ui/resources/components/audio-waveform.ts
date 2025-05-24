// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  classMap,
  css,
  html,
  nothing,
  PropertyDeclarations,
  repeat,
  svg,
} from 'chrome://resources/mwc/lit/index.js';

import {
  POWER_SCALE_FACTOR,
  SAMPLE_RATE,
  SAMPLES_PER_SLICE,
} from '../core/audio_constants.js';
import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {computed} from '../core/reactive/signal.js';
import {Transcription} from '../core/soda/soda.js';
import {
  assert,
  assertExists,
  assertInstanceof,
} from '../core/utils/assert.js';
import {InteriorMutableArray} from '../core/utils/interior_mutable_array.js';

import {
  getNumSpeakerClass,
  getSpeakerLabelClass,
  SPEAKER_LABEL_COLORS,
} from './styles/speaker_label.js';

const BAR_WIDTH = 4;
const BAR_GAP = 5;
const BAR_MIN_HEIGHT = 4.5;
const BAR_MAX_HEIGHT = 100;

const SPEAKER_LABEL_LINE_HEIGHT = 128;

// We don't use DOMRect since it's much slower.
interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

// TODO(pihsun): Is there some way to set .viewBox.baseVal?
function toViewBoxString(viewBox: Rect|null): string|typeof nothing {
  if (viewBox === null) {
    return nothing;
  }
  const {x, y, width, height} = viewBox;
  return `${x} ${y} ${width} ${height}`;
}

/*
 * There are multiple different coordinate system for the "timestamp" of the
 * waveform used in this component:
 * (1) Time (in seconds). Each second contains SAMPLE_RATE audio samples.
 * (2) Index of the "bar" in the waveform, starting from 0. Each "bar" is an
 *     aggregate of SAMPLES_PER_SLICE audio samples. So index 0 corresponds to
 *     [0, SAMPLES_PER_SLICE) audio samples, index 1 corresponds to
 *     [SAMPLES_PER_SLICE, 2*SAMPLES_PER_SLICE) audio samples, and so on...
 * (3) The x coordinate that is rendered in the SVG. Time 0 always corresponds
 *     to x = 0, and the viewBox of the whole SVG is set to show around the
 *     current time.
 *
 * `timestampToBarIndex` converts from (1) to (2), `getBarX` converts from
 * (2) to (3), and `xCoordinateToRoughIdx` converts from (3) to (2).
 *
 * Since the whole waveform looks better when things are aligned to bar, most
 * variables (ended in BarIdx) are in the coordinate of (2).
 *
 * Also note that to render separator between bars, sometimes the "index" in (2)
 * have 0.5 in fractions, but those values should only be used to convert to
 * rendered x coordinate (3) and doesn't corresponds to actual slice of audio
 * samples.
 */
function timestampToBarIndex(seconds: number): number {
  return Math.floor((seconds * SAMPLE_RATE) / SAMPLES_PER_SLICE);
}

function getBarX(barIdx: number): number {
  return barIdx * (BAR_WIDTH + BAR_GAP);
}

function xCoordinateToRoughIdx(x: number): number {
  return Math.floor(x / (BAR_WIDTH + BAR_GAP));
}

/**
 * Range of bars that should have the same speaker label.
 *
 * The range is [startBarIdx, endBarIdx), that is it includes startBarIdx but
 * excludes endBarIdx.
 *
 * Note that since each "bar" corresponds to several samples, and the speaker
 * label can be different during those samples, so we define each "bar" to have
 * speaker label as the speaker label at the end of the time range that the bar
 * corresponds to.
 */
interface SpeakerLabelRange {
  startBarIdx: number;
  endBarIdx: number;
  speakerLabelIndex: number;
}

interface SpeakerLabelInfo {
  speakerLabels: string[];

  /**
   * The ranges of speaker labels.
   *
   * The ranges are sorted and non-overlapping, and there could be gaps (part of
   * audio without speaker label) between ranges.
   */
  ranges: SpeakerLabelRange[];
}

/**
 * Component for showing audio waveform.
 */
export class AudioWaveform extends ReactiveLitElement {
  static override styles = [
    SPEAKER_LABEL_COLORS,
    css`
      :host {
        display: block;
        position: relative;
      }

      #chart {
        inset: 0;
        position: absolute;
      }

      .speaker-single {
        & .range {
          display: none;
        }
      }

      .speaker-duo,
      .speaker-multiple {
        & .no-speaker {
          --speaker-label-shapes-color: var(--cros-sys-primary_container);
        }
      }

      .speaker-range-start {
        /* The dash and space looks equal length with rounded linecap. */
        stroke-dasharray: 2, 6;
        stroke-linecap: round;
        stroke-width: 2;
        stroke: var(--speaker-label-shapes-color);

        .speaker-single & {
          display: none;
        }

        &.future {
          opacity: var(--cros-disabled-opacity);
        }

        .range:hover & {
          stroke-dasharray: none;
        }
      }

      .bar {
        /* Don't block hover on the background. */
        pointer-events: none;

        .speaker-single & {
          fill: var(--cros-sys-primary);

          &.future {
            fill: var(--cros-sys-primary_container);
          }
        }

        :is(.speaker-duo, .speaker-multiple) & {
          fill: var(--speaker-label-shapes-color);

          &.future {
            opacity: var(--cros-disabled-opacity);
          }
        }
      }

      .background {
        /* fill: none prevents :hover state, so we set opacity: 0 instead. */
        opacity: 0;
        fill: var(--speaker-label-container-color);

        .range:hover & {
          opacity: 1;

          &.future {
            opacity: var(--cros-disabled-opacity);
          }
        }
      }

      .speaker-label {
        align-items: center;
        background: var(--speaker-label-shapes-color);
        border-radius: 10px 10px 10px 0;
        bottom: 0;
        box-sizing: border-box;
        color: var(--speaker-label-label-color);
        display: flex;
        font: var(--cros-label-1-font);
        height: 20px;
        justify-content: center;
        left: 0;
        min-width: 20px;
        padding: 4px;
        position: absolute;
        width: fit-content;

        &.outside {
          display: none;
        }

        & > .full {
          display: none;
        }

        .range:hover & {
          display: block;

          /* TODO: b/336963138 - Animation on hover? */
          height: 26px;
          padding: 8px;

          & > .full {
            display: inline;
          }

          & > .short {
            display: none;
          }
        }
      }

      .playhead {
        fill: var(--cros-sys-on_surface_variant);

        /* Don't block hover on the background. */
        pointer-events: none;
      }
    `,
  ];

  static override properties: PropertyDeclarations = {
    values: {attribute: false},
    size: {state: true},
    currentTime: {type: Number},
    transcription: {attribute: false},
  };

  // Values to be shown as bars. Should be in range [0, POWER_SCALE_FACTOR - 1].
  values = new InteriorMutableArray<number>([]);

  currentTime: number|null = null;

  private readonly currentTimeSignal = this.propSignal('currentTime');

  private readonly currentTimeBarIdx = computed(() => {
    if (this.currentTimeSignal.value === null) {
      return null;
    }
    return timestampToBarIndex(this.currentTimeSignal.value);
  });

  private size: DOMRect|null = null;

  transcription: Transcription|null = null;

  private readonly transcriptionSignal = this.propSignal('transcription');

  private readonly speakerLabelInfo = computed((): SpeakerLabelInfo => {
    const transcription = this.transcriptionSignal.value;
    if (transcription === null) {
      return {
        speakerLabels: [],
        ranges: [],
      };
    }
    const paragraphs = transcription.getParagraphs();
    const speakerLabels = transcription.getSpeakerLabels();
    const ranges: SpeakerLabelRange[] = [];
    for (const paragraph of paragraphs) {
      const firstPart = assertExists(paragraph[0]);
      const lastPart = assertExists(paragraph.at(-1));

      const speakerLabel = firstPart.speakerLabel;
      if (speakerLabel === null) {
        // The paragraph doesn't have speaker label.
        continue;
      }
      const speakerLabelIndex = speakerLabels.indexOf(speakerLabel);
      assert(speakerLabelIndex !== -1);

      const startMs = firstPart.timeRange?.startMs ?? null;
      const endMs = lastPart.timeRange?.endMs ?? null;
      if (startMs === null || endMs === null) {
        // TODO(pihsun): Check if there's any possibility that the timestamp is
        // missing.
        continue;
      }
      // The timestamps should be increasing.
      assert(startMs <= endMs);

      const startBarIdx = timestampToBarIndex(startMs / 1000);
      const endBarIdx = timestampToBarIndex(endMs / 1000);
      assert(
        ranges.length === 0 ||
          assertExists(ranges.at(-1)).endBarIdx <= startBarIdx,
      );
      // These can be equal if there's a very short paragraph with speaker
      // label.
      if (startBarIdx !== endBarIdx) {
        ranges.push({
          speakerLabelIndex,
          startBarIdx,
          endBarIdx,
        });
      }
    }
    return {
      speakerLabels,
      ranges,
    };
  });

  private readonly resizeObserver = new ResizeObserver(() => {
    this.size = this.getBoundingClientRect();
  });

  get chart(): SVGElement {
    return assertInstanceof(
      assertExists(this.shadowRoot).querySelector('#chart'),
      SVGElement,
    );
  }

  // TODO(pihsun): Check if we can use ResizeObserver in @lit-labs/observers.
  override connectedCallback(): void {
    super.connectedCallback();
    this.resizeObserver.observe(this);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.resizeObserver.disconnect();
  }

  private getBarLocation(
    idx: number,
    val: number,
    minHeight: number,
    maxHeight: number,
  ): Rect {
    const width = BAR_WIDTH;
    const height =
      minHeight + (maxHeight - minHeight) * (val / (POWER_SCALE_FACTOR - 1));
    const x = getBarX(idx) - width / 2;
    const y = -height / 2;

    return {x, y, width, height};
  }

  private isAfterCurrentTime(idx: number) {
    return (
      this.currentTimeBarIdx.value !== null &&
      idx >= this.currentTimeBarIdx.value
    );
  }

  private renderSpeakerRangeStart({
    startBarIdx,
    speakerLabelIndex,
  }: SpeakerLabelRange) {
    const startX = getBarX(startBarIdx - 0.5);

    const classes = {
      [getSpeakerLabelClass(speakerLabelIndex)]: true,
      future: this.isAfterCurrentTime(startBarIdx),
    };
    const height = SPEAKER_LABEL_LINE_HEIGHT;
    // clang-format off
    return svg`<line
      x1=${startX}
      x2=${startX}
      y1=${-height / 2}
      y2=${height / 2}
      class="speaker-range-start ${classMap(classes)} "
    />`;
    // clang-format on
  }

  private renderSpeakerRangeLabel(
    speakerLabels: string[],
    {startBarIdx, speakerLabelIndex}: SpeakerLabelRange,
    viewBox: Rect,
  ) {
    // minus one so it aligns with the left edge of the speaker label range
    // start.
    const startX = getBarX(startBarIdx - 0.5) - 1;

    const classes = {
      [getSpeakerLabelClass(speakerLabelIndex)]: true,
      outside: startX < viewBox.x,
    };

    const maxHeight = 26;
    // Always render the label in view. It'll be hidden until hover if it's
    // originally outside of the view. Note that only the label that has some
    // corresponding bar inside view will be rendered (see renderSvgContent).
    const x = Math.max(startX, viewBox.x);
    const y = -SPEAKER_LABEL_LINE_HEIGHT / 2 - maxHeight;
    const shortLabel = assertExists(speakerLabels[speakerLabelIndex]);
    const fullLabel = i18n.transcriptionSpeakerLabelLabel(shortLabel);

    // clang-format off
    // The width/height on foreignObject is necessary for the div to be shown,
    // but the actual label size can be smaller than that.
    // TODO(pihsun): This introduce a bit more hover space than the visible
    // labels. Check if there's a better way to do this.
    return svg`<foreignObject
      x=${x}
      y=${y}
      width="100"
      height=${maxHeight}
    >
      <div class="speaker-label ${classMap(classes)}" aria-label=${fullLabel}>
        <span class="short" aria-hidden="true">${shortLabel}</span>
        <span class="full" aria-hidden="true">${fullLabel}</span>
      </div>
    </foreignObject>`;
    // clang-format on
  }

  /**
   * Returns the background path with the top-right and bottom-right corner
   * rounded.
   */
  private getBackgroundPath(startX: number, endX: number) {
    const height = SPEAKER_LABEL_LINE_HEIGHT;
    const radius = 12;
    // clang-format off
    return `
      M ${startX} ${-height / 2}
      v ${height}
      H ${endX - radius}
      a ${radius} ${radius} 0 0 0 ${radius} ${-radius}
      V ${-height / 2 + radius}
      a ${radius} ${radius} 0 0 0 ${-radius} ${-radius}
      H ${startX}
    `;
    // clang-format on
  }

  private renderSpeakerRangeBackground({
    startBarIdx,
    endBarIdx,
    speakerLabelIndex,
  }: SpeakerLabelRange) {
    const startX = getBarX(startBarIdx - 0.5);
    const endX = getBarX(endBarIdx - 0.5);

    const classes: Record<string, boolean> = {
      [getSpeakerLabelClass(speakerLabelIndex)]: true,
    };

    const currentTimeIdx = this.currentTimeBarIdx.value;
    if (currentTimeIdx !== null && startBarIdx <= currentTimeIdx &&
        currentTimeIdx < endBarIdx) {
      // Part of the background are before and part are after. Need to cut the
      // background in half.
      const centerX = getBarX(currentTimeIdx) - BAR_WIDTH / 2;
      const height = SPEAKER_LABEL_LINE_HEIGHT;
      const y = -height / 2;
      return [
        svg`<rect
          x=${startX}
          y=${y}
          width=${centerX - startX}
          height=${height}
          class="background ${classMap(classes)}"
        />`,
        svg`<path
          d=${this.getBackgroundPath(centerX, endX)}
          class="background future ${classMap(classes)}"
        />`,
      ];
    } else {
      classes['future'] = this.isAfterCurrentTime(startBarIdx);
      return svg`<path
        d=${this.getBackgroundPath(startX, endX)}
        class="background ${classMap(classes)}"
      />`;
    }
  }

  private renderSpeakerRange(
    speakerLabels: string[],
    range: SpeakerLabelRange,
    viewBox: Rect,
  ) {
    return svg`<g class="range">
      ${this.renderSpeakerRangeBackground(range)}
      ${this.renderSpeakerRangeStart(range)}
      ${this.renderSpeakerRangeLabel(speakerLabels, range, viewBox)}
    </g>`;
  }

  private renderCurrentTimeBar(viewBox: Rect) {
    if (this.currentTimeBarIdx.value === null) {
      return nothing;
    }
    const width = 2;
    // Add the progress indicator at the current time. Draw on the left side
    // of the current bar so it looks more "correct" when jumping to the start
    // of a paragraph with speaker label.
    const x = getBarX(this.currentTimeBarIdx.value) - BAR_WIDTH / 2 - width;
    const y = viewBox.y;
    return svg`<rect
      x=${x}
      y=${y}
      width=${width}
      height=${viewBox.height}
      rx="1"
      class="playhead"
    />`;
  }

  private renderAudioBars(viewBox: Rect) {
    if (this.values.length === 0) {
      return nothing;
    }

    const speakerLabelRanges = this.speakerLabelInfo.value.ranges;
    let currentSpeakerLabelRangeIdx = 0;
    let currentSpeakerLabelRangeRendered = false;

    /**
     * Gets the speaker label index of a bar index.
     *
     * The `barIdx` given to this function needs to be increasing across
     * multiple calls, since this is implemented by scanning through the
     * speakerLabelRanges.
     */
    function getSpeakerLabelRange(barIdx: number): SpeakerLabelRange|null {
      while (currentSpeakerLabelRangeIdx < speakerLabelRanges.length) {
        const range = assertExists(
          speakerLabelRanges[currentSpeakerLabelRangeIdx],
        );
        if (barIdx < range.startBarIdx) {
          return null;
        }
        if (barIdx < range.endBarIdx) {
          return range;
        }
        currentSpeakerLabelRangeIdx += 1;
        currentSpeakerLabelRangeRendered = false;
      }
      return null;
    }

    // This is an optimization to not goes through the whole values array, and
    // directly calculate the part that needs to be rendered instead. To
    // simplify the logic we calculate the rough range and just extend it a bit
    // to make sure we covers the whole range.
    const startIdx = Math.max(xCoordinateToRoughIdx(viewBox.x) - 5, 0);
    const endIdx = Math.min(
      xCoordinateToRoughIdx(viewBox.x + viewBox.width) + 5,
      this.values.length - 1,
    );

    if (endIdx < startIdx) {
      return nothing;
    }

    const idxRange = Array.from(
      {length: endIdx - startIdx + 1},
      (_, i) => i + startIdx,
    );

    return repeat(
      idxRange,
      (i) => i,
      (i) => {
        const ret: RenderResult[] = [];

        const val = assertExists(this.values.array[i]);
        const rect = this.getBarLocation(
          i,
          val,
          BAR_MIN_HEIGHT,
          Math.min(viewBox.height, BAR_MAX_HEIGHT),
        );
        if (rect.x + rect.width < viewBox.x ||
            rect.x > viewBox.x + viewBox.width) {
          return nothing;
        }
        const classes: Record<string, boolean> = {
          future: this.isAfterCurrentTime(i),
        };
        const range = getSpeakerLabelRange(i);

        if (range !== null) {
          if (!currentSpeakerLabelRangeRendered) {
            ret.push(
              this.renderSpeakerRange(
                this.speakerLabelInfo.value.speakerLabels,
                range,
                viewBox,
              ),
            );
            currentSpeakerLabelRangeRendered = true;
          }

          classes[getSpeakerLabelClass(range.speakerLabelIndex)] = true;
        } else {
          classes['no-speaker'] = true;
        }
        ret.push(svg`<rect
          x=${rect.x}
          y=${rect.y}
          width=${rect.width}
          height=${rect.height}
          rx=${rect.width / 2}
          class="bar ${classMap(classes)}"
        />`);
        return ret;
      },
    );
  }

  private renderSvgContent(viewBox: Rect|null) {
    if (viewBox === null) {
      return nothing;
    }
    return [this.renderAudioBars(viewBox), this.renderCurrentTimeBar(viewBox)];
  }

  private getViewBox(): Rect|null {
    if (this.size === null) {
      return null;
    }
    const {width, height} = this.size;
    const x = (() => {
      if (this.currentTimeBarIdx.value !== null) {
        const x = getBarX(this.currentTimeBarIdx.value);
        // Put the current time in the center.
        // TODO(pihsun): Should this be controlled by a separate property?
        // TODO(pihsun): Should we use the real time offset, instead of
        // aligning to the bar?
        return x - width / 2;
      } else {
        return this.values.length * (BAR_WIDTH + BAR_GAP) - width;
      }
    })();
    const y = -height / 2;
    return {x, y, width, height};
  }

  override render(): RenderResult {
    if (this.size === null) {
      return nothing;
    }

    const numSpeakerClass = getNumSpeakerClass(
      this.speakerLabelInfo.value.speakerLabels.length,
    );

    const viewBox = this.getViewBox();

    // TODO(pihsun): Performance doesn't seem to be ideal for rendering this
    // with svg. Measure it for longer recording and see if there's other way to
    // do it. (Draw on canvas directly?)
    return html`<svg
      id="chart"
      viewBox=${toViewBoxString(viewBox)}
      class=${numSpeakerClass}
    >
      ${this.renderSvgContent(viewBox)}
    </svg>`;
  }
}

window.customElements.define('audio-waveform', AudioWaveform);

declare global {
  interface HTMLElementTagNameMap {
    'audio-waveform': AudioWaveform;
  }
}
