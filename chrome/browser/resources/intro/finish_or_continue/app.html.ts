// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {FinishOrContinueAppElement} from './app.js';

export function getHtml(this: FinishOrContinueAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<img id="product-logo" src="images/product-logo.svg" alt="">
<h1 class="title">$i18n{finishOrContinueTitle}</h1>
<div id="buttonContainer">
  <if expr="not is_win">
    <cr-button id="continueEducationButton">
      ${this.getContinueEducationButtonLabel_()}
    </cr-button>
  </if>
  <cr-button id="startBrowsingButton" class="action-button">
    $i18n{startBrowsingButtonLabel}
  </cr-button>
  <if expr="is_win">
    <cr-button id="continueEducationButton">
      ${this.getContinueEducationButtonLabel_()}
    </cr-button>
  </if>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
