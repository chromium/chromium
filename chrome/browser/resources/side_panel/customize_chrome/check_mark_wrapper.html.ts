// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CheckMarkWrapperElement} from './check_mark_wrapper.js';

export function getHtml(this: CheckMarkWrapperElement) {
  return html`<!--_html_template_start_-->
<div id="circle">
  <svg id="svg" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48">
    <path id="checkMark" d="M20 34 10 24l2.83-2.83L20 28.34l15.17-15.17L38 16Z">
    </path>
  </svg>
</div>
<slot></slot>
<!--_html_template_end_-->`;
}
