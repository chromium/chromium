// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  classMap,
  css,
  html,
  LitElement,
  nothing,
  PropertyDeclarations,
  svg,
  SVGTemplateResult,
} from 'chrome://resources/mwc/lit/index.js';

import {
  POWER_SCALE_FACTOR,
  SAMPLE_RATE,
  SAMPLES_PER_SLICE,
} from '../core/audio_constants.js';
import {assertExists, assertInstanceof} from '../core/utils/assert.js';

const BAR_WIDTH = 4;
const BAR_GAP = 5;
const BAR_MIN_HEIGHT = 4.5;
const BAR_MAX_HEIGHT = 100;

// We don't use DOMRect since it's much slower.
interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

/**
 * Component for showing audio waveform.
 */
export class AudioWaveform extends LitElement {
  static override styles = css`
    :host {
      display: block;
      position: relative;
    }

    #chart {
      fill: var(--cros-sys-primary);
      inset: 0;
      position: absolute;
    }

    .after {
      fill: var(--cros-sys-primary_container);
    }

    .playhead {
      fill: var(--cros-sys-on_surface_variant);
    }
  `;

  static override properties: PropertyDeclarations = {
    values: {attribute: false},
    size: {state: true},
    currentTime: {type: Number},
  };

  // Values to be shown as bars. Should be in range [0, POWER_SCALE_FACTOR - 1].
  values: number[] = [];

  currentTime: number|null = null;

  private size: DOMRect|null = null;

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

  // TODO(pihsun): Should this be in recording_session instead?
  private timestampToBarIndex(seconds: number) {
    return Math.floor((seconds * SAMPLE_RATE) / SAMPLES_PER_SLICE);
  }

  // TODO(pihsun): Should this be in recording_session instead?
  private barIndexToTimestamp(idx: number) {
    return (idx * SAMPLES_PER_SLICE) / SAMPLE_RATE;
  }

  private getBarX(idx: number): number {
    return idx * (BAR_WIDTH + BAR_GAP);
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
    const x = this.getBarX(idx) - width / 2;
    const y = -height / 2;

    return {x, y, width, height};
  }

  private getSvgContent(): SVGTemplateResult[]|typeof nothing {
    if (this.size === null || this.values.length === 0) {
      return nothing;
    }
    const viewBox = assertExists(this.getViewBox());
    const ret: SVGTemplateResult[] = [];
    for (const [i, val] of this.values.entries()) {
      const rect = this.getBarLocation(
        i,
        val,
        BAR_MIN_HEIGHT,
        Math.min(this.size.height, BAR_MAX_HEIGHT),
      );
      // TODO(pihsun): Optimize and directly calculate the index range.
      if (rect.x + rect.width < viewBox.x ||
          rect.x > viewBox.x + viewBox.width) {
        continue;
      }
      const classes = {
        after: this.currentTime !== null &&
          this.barIndexToTimestamp(i) >= this.currentTime,
      };
      ret.push(
        svg`<rect
          x=${rect.x}
          y=${rect.y}
          width=${rect.width}
          height=${rect.height}
          rx=${rect.width / 2}
          class=${classMap(classes)}
        />`,
      );
    }
    if (this.currentTime !== null) {
      // Add the progress indicator at the current time.
      const idx = this.timestampToBarIndex(this.currentTime);
      const x = this.getBarX(idx) - 1;
      const y = -this.size.height / 2;
      ret.push(
        svg`<rect
          x=${x}
          y=${y}
          width="2"
          height=${this.size.height}
          rx="1"
          class="playhead"
        />`,
      );
    }
    return ret;
  }

  private getViewBox(): Rect|null {
    if (this.size === null) {
      return null;
    }
    const {width, height} = this.size;
    const x = (() => {
      if (this.currentTime !== null) {
        const idx = this.timestampToBarIndex(this.currentTime);
        const x = this.getBarX(idx);
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

  // TODO(pihsun): Is there some way to set .viewBox.baseVal?
  private getViewBoxString(): string|typeof nothing {
    if (this.size === null) {
      return nothing;
    }
    const {x, y, width, height} = assertExists(this.getViewBox());
    return `${x} ${y} ${width} ${height}`;
  }

  override render(): RenderResult {
    if (this.size === null) {
      return nothing;
    }
    // TODO(pihsun): Performance doesn't seem to be ideal for rendering this
    // with svg. Measure it for longer recording and see if there's other way to
    // do it. (Draw on canvas directly?)
    return html`<svg id="chart" viewBox=${this.getViewBoxString()}>
      ${this.getSvgContent()}
    </svg>`;
  }
}

window.customElements.define('audio-waveform', AudioWaveform);

declare global {
  interface HTMLElementTagNameMap {
    'audio-waveform': AudioWaveform;
  }
}
