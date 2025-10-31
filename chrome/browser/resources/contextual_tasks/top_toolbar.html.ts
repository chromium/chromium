// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TopToolbarElement} from './top_toolbar.js';

// clang-format off
export function getHtml(this: TopToolbarElement) {
  return html`<!--_html_template_start_-->
  Contextual Tasks UI
  <div id="rightButtonContainer">
    <!-- TODO(crbug.com/454388385): Remove this once the authentication flow
        is implemented. -->
    <button @click="${this.onSigninClick_}">Press for sign in</button>
  </div>
  <!--_html_template_end_-->`;
}
// clang-format on
