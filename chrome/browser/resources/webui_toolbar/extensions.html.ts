// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsElement} from './extensions.js';

export function getHtml(this: ExtensionsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${repeat(
    this.keyedStates_,
    (keyedState) => keyedState.key,
    (keyedState) => html`
      <webui-toolbar-extension
          .state="${keyedState.state}"
          class="${keyedState.animateIn ? 'animate-in' : ''}
                 ${keyedState.exiting ? 'exiting' : ''}"
          data-key="${keyedState.key}">
      </webui-toolbar-extension>
    `,
)}
${this.keyedStates_.length > 0 ? html`
  <toolbar-divider
      class="${this.animateInDivider_() ? 'animate-in' : ''}
             ${this.allExiting_() ? 'exiting' : ''}">
  </toolbar-divider>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
