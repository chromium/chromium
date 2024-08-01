// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_slider/cr_slider.js';

import type {CrSliderElement, SliderTick} from '//resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './config_slider.html.js';

function createTicks(start: number, end: number, step: number): SliderTick[] {
  const ticks: SliderTick[] = [];
  for (let tickValue = start; tickValue <= end; tickValue += step) {
    ticks.push({
      label: `${tickValue}`,
      value: tickValue,
    });
  }
  return ticks;
}

export interface HealthdInternalsConfigSliderElement {
  $: {
    tickSlider: CrSliderElement,
  };
}

export class HealthdInternalsConfigSliderElement extends PolymerElement {
  static get is() {
    return 'healthd-internals-config-slider';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      sliderTitle: {type: String},
      ticks: {type: Array},
      markerCounts: {type: Number},
      tickedIndex: {type: Number},
    };
  }

  // Set in `initTitle`.
  private sliderTitle: string = '';

  // Set in `initSlider`.
  private ticks: SliderTick[] = [];
  private markerCounts: number = 0;
  private startTick: number = 0;
  private tickStepSize: number = 0;

  // Set in `setTickValue`.
  private tickedIndex: number = 0;

  initTitle(title: string) {
    this.sliderTitle = title;
  }

  initSlider(start: number, end: number, stepSize: number) {
    this.ticks = createTicks(start, end, stepSize);
    // The width of slider is fixed to 200px. Hide tick markers when there are
    // too many. Otherwise, there is not enough space between tick markers.
    this.markerCounts = (this.ticks.length > 20) ? 0 : this.ticks.length;
    this.startTick = start;
    this.tickStepSize = stepSize;
  }

  setTickValue(actualValue: number) {
    this.tickedIndex = (actualValue - this.startTick) / this.tickStepSize;
  }

  getTickValue(): number {
    return this.ticks[this.tickedIndex]!.value;
  }

  private onTickedValueChanged() {
    this.tickedIndex = this.$.tickSlider.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'healthd-internals-config-slider': HealthdInternalsConfigSliderElement;
  }
}

customElements.define(
    HealthdInternalsConfigSliderElement.is,
    HealthdInternalsConfigSliderElement);
