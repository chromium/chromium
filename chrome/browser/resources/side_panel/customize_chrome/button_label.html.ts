// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ButtonLabelElement} from './button_label.js';

export function getHtml(this: ButtonLabelElement) {
  return html`<!--_html_template_start_-->
<div id="label">${this.label}</div>
<div id="labelDescription" ?hidden="${!this.labelDescription}">
  ${this.labelDescription}
</div>
<!--_html_template_end_-->`;
}
