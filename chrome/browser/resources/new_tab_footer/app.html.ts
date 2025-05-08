// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NewTabFooterAppElement} from './app.js';

export function getHtml(this: NewTabFooterAppElement) {
  // clang-format off
  return html`
<!--
Container for housing the items in the center of the footer that are
separated from each other by a divider.
-->
<div id="centerContainer">
  ${this.managementNotice_ ?
      html`<div id="managementNoticeText"><p>${this.managementNotice_.text}</p></div>` : ''}
  ${this.extensionAttribution_ ?
      html`<div id="extensionAttribution">
        <a href="${this.extensionAttribution_.url}">
            ${this.extensionAttribution_.name}
        </a>
      </div>` : ''}
</div>`;
}