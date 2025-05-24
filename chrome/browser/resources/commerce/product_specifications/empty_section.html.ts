// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TooltipPosition} from '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {EmptySectionElement} from './empty_section.js';

export function getHtml(this: EmptySectionElement) {
  return html`<!--_html_template_start_-->
  <div id="dash">—</div>
  <cr-tooltip id="tooltip" for="dash"
      position="${TooltipPosition.TOP}" offset="0" animation-delay="0"
      fit-to-visible-bounds>
    $i18n{notAvailableTooltip}
  </cr-tooltip>
  <!--_html_template_end_-->`;
}
