// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PriceTrackingSectionElement} from './price_tracking_section.js';

export function getHtml(this: PriceTrackingSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="toggleSection">
  <div id="toggleText">
    <div id="toggleTitle">$i18n{trackPriceTitle}</div>
    <!-- This is a special handling to make sure that the annotation text,
    folder name and period will be in the same line. -->
    <div id="toggleAnnotation">
      ${this.toggleAnnotationText_}
      <span ?hidden="${!this.showSaveLocationText_}">
        ${this.saveLocationStartText_}<a href="#"
            id="toggleAnnotationButton"
            @click="${this.onToggleAnnotationButtonClick_}"
            >${this.folderName_}</a>${this.saveLocationEndText_}
      </span>
    </div>
  </div>
  <cr-toggle id="toggle" .checked="${this.isProductTracked}"
      @checked-changed="${this.onToggleCheckedChanged_}"
      @change="${this.onToggleChange_}" title="$i18n{trackPriceTitle}"
      aria-describedby="toggleAnnotation">
  </cr-toggle>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
