// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';
import './ink_brush_selector.js';
import './ink_color_selector.js';
import './ink_size_selector.js';
import './viewer_bottom_toolbar_dropdown.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerBottomToolbarElement} from './viewer_bottom_toolbar.js';

export function getHtml(this: ViewerBottomToolbarElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <ink-brush-selector class="toolbar-icon" .currentType="${this.currentType}"
        @current-type-changed="${this.onCurrentTypeChanged}">
    </ink-brush-selector>
    <span id="vertical-separator"></span>
    ${this.shouldShowBrushOptions_() ? html`
      <viewer-bottom-toolbar-dropdown id="size" class="toolbar-icon"
          .buttonTitle="${this.getSizeTitle_()}">
        <cr-icon slot="icon" icon="${this.getSizeIcon_()}"></cr-icon>
        <ink-size-selector slot="menu" .currentSize="${this.currentSize}"
            .currentType="${this.currentType}"
            @current-size-changed="${this.onCurrentSizeChanged}">
        </ink-size-selector>
      </viewer-bottom-toolbar-dropdown>
      <viewer-bottom-toolbar-dropdown id="color" class="toolbar-icon"
          .buttonTitle="${this.getColorTitle_()}">
        <div slot="icon" class="color-chip"></div>
        <ink-color-selector slot="menu" label="$i18n{ink2Color}"
            .colors="${this.availableBrushColors()}"
            .currentColor="${this.currentColor}"
            @current-color-changed="${this.onCurrentColorChanged}">
        </ink-color-selector>
      </viewer-bottom-toolbar-dropdown>` : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
