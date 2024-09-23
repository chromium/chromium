// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DiceAppElement} from './dice_app.js';

export function getHtml(this: DiceAppElement) {
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager">
  <div id="splash" slot="view">
    <img id="product-logo-animation" src="images/product-logo-animation.svg">
  </div>
  <sign-in-promo id="signInPromo" slot="view"></sign-in-promo>
</cr-view-manager>
<!--_html_template_end_-->`;
}
