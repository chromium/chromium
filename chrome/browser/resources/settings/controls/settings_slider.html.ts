// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSliderElement} from './settings_slider.js';

export function getHtml(this: SettingsSliderElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.pref?.controlledBy ? html`
  <cr-policy-pref-indicator .pref="${this.pref}"></cr-policy-pref-indicator>
` : ''}
<div class="outer">
  <cr-slider id="slider"
      ?disabled="${this.disableSlider_}"
      .ticks="${this.ticks}"
      .value="${this.sliderValue_}"
      .markerCount="${this.markerCount_}"
      ?show-markers="${this.showMarkers}"
      .max="${this.max ?? 100}"
      .min="${this.min ?? 0}"
      @cr-slider-value-changed="${this.onCrSliderValueChanged_}"
      @dragging-changed="${this.onDraggingChanged_}"
      @updating-from-key-changed="${this.onUpdatingFromKeyChanged_}"
      aria-roledescription="${this.getRoleDescription_()}"
      aria-label="${this.labelAria}"
      aria-disabled="${this.ariaDisabled}">
  </cr-slider>
  <!-- aria-hidden because role description on #slider contains min/max. -->
  <div id="labels" ?disabled="${this.disableSlider_}"
      aria-hidden="true">
    <div id="label-begin">${this.labelMin}</div>
    <div id="label-end">${this.labelMax}</div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
