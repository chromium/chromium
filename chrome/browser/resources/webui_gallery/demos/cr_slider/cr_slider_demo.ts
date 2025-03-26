// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';

import type {CrSliderElement, SliderTick} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_slider_demo.css.js';
import {getHtml} from './cr_slider_demo.html.js';

function createTicks(
    min: number, increment: number, steps: number): SliderTick[] {
  const ticks: SliderTick[] = [];
  for (let i = min; i <= steps; i++) {
    const tickValue = min + (i * increment);
    ticks.push({
      label: `${tickValue}`,
      value: tickValue,
    });
  }
  return ticks;
}

export interface CrSliderDemoElement {
  $: {
    basicSlider: CrSliderElement,
    tickedSlider: CrSliderElement,
  };
}

export class CrSliderDemoElement extends CrLitElement {
  static get is() {
    return 'cr-slider-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      basicValue_: {type: Number},
      showMarkers_: {type: Boolean},
      tickedValue_: {type: Number},
      ticks_: {type: Array},
    };
  }

  protected accessor basicValue_: number = 5;
  protected accessor showMarkers_: boolean = false;
  protected accessor tickedValue_: number = 0;
  protected accessor ticks_: SliderTick[] = createTicks(0, 5, 5);

  protected getMarkerCount_(): number {
    if (!this.showMarkers_) {
      return 0;
    }

    return this.ticks_.length;
  }

  protected getTickValue_(): number {
    return this.ticks_[this.tickedValue_]!.value;
  }

  protected onBasicValueChanged_() {
    this.basicValue_ = this.$.basicSlider.value;
  }

  protected onTickedValueChanged_() {
    this.tickedValue_ = this.$.tickedSlider.value;
  }

  protected onShowMarkersChanged_(e: CustomEvent<{value: boolean}>) {
    this.showMarkers_ = e.detail.value;
  }
}

export const tagName = CrSliderDemoElement.is;

customElements.define(CrSliderDemoElement.is, CrSliderDemoElement);
