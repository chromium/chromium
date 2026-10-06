// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LeftHandSideElement} from './left_hand_side.js';

export function getHtml(this: LeftHandSideElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  <ntp-iframe id="expanded"
      src="chrome-untrusted://new-tab-page/expanded-lhs">
  </ntp-iframe>
  <!--_html_template_end_-->`;
  // clang-format on
}
