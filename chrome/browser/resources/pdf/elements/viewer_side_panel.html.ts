// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerSidePanelElement} from './viewer_side_panel.js';

export function getHtml(this: ViewerSidePanelElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <ink-brush-selector .currentType="${this.currentType}"
        @current-type-changed="${this.onCurrentTypeChanged}">
    </ink-brush-selector>
    ${this.shouldShowBrushOptions_() ? html`
      <div id="brush-options" class="side-panel-content">
        <h2>$i18n{ink2Size}</h2>
        <ink-size-selector .currentSize="${this.currentSize}"
            .currentType="${this.currentType}"
            @current-size-changed="${this.onCurrentSizeChanged}">
        </ink-size-selector>
        <h2>$i18n{ink2Color}</h2>
        <ink-color-selector label="$i18n{ink2Color}"
            .colors="${this.availableBrushColors()}"
            .currentColor="${this.currentColor}"
            @current-color-changed="${this.onCurrentColorChanged}">
        </ink-color-selector>
      </div>` : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
