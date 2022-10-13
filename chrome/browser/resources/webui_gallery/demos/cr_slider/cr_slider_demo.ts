// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';

import {CrSliderElement, SliderTick} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_slider_demo.html.js';

interface CrSliderDemoElement {
  $: {
    basicSlider: CrSliderElement,
    tickedSlider: CrSliderElement,
  };
}

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

class CrSliderDemoElement extends PolymerElement {
  static get is() {
    return 'cr-slider-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      basicValue_: Number,
      disabledTicks_: Array,
      showMarkers_: Boolean,
      tickedValue_: Number,
      ticks_: Array,
    };
  }

  private basicValue_: number = 5;
  private showMarkers_: boolean = false;
  private tickedValue_: number = 0;
  private ticks_: SliderTick[] = createTicks(0, 5, 5);

  private getMarkerCount_(): number {
    if (!this.showMarkers_) {
      return 0;
    }

    return this.ticks_.length;
  }

  private getTickValue_(): number {
    return this.ticks_[this.tickedValue_]!.value;
  }

  private onBasicValueChanged_() {
    this.basicValue_ = this.$.basicSlider.value;
  }

  private onTickedValueChanged_() {
    this.tickedValue_ = this.$.tickedSlider.value;
  }
}

customElements.define(CrSliderDemoElement.is, CrSliderDemoElement);
