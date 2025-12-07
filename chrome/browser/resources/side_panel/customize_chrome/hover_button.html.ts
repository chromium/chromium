// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import {nothing} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {HoverButtonElement} from './hover_button.js';

export function getHtml(this: HoverButtonElement) {
  return html`<!--_html_template_start_-->
<div id="hoverButton" role="button" tabindex="0">
  <customize-chrome-button-label label="${this.label}"
      label-description="${this.labelDescription || nothing}"
      aria-label="${this.ariaButtonLabel || ''}">
  </customize-chrome-button-label>
  <div id="icon" class="cr-icon"></div>
</div>
<!--_html_template_end_-->`;
}
