// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabSearchGroupItemElement} from './tab_search_group_item.js';

export function getHtml(this: TabSearchGroupItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="iconContainer">
  <!-- TODO(romanarora): Replace with a 16x16 SVG. -->
  <svg id="icon" width="32" height="32" viewBox="0 0 32 32" fill="none"
      xmlns="http://www.w3.org/2000/svg">
    <mask id="mask0" mask-type="alpha" maskUnits="userSpaceOnUse" x="2" y="4"
        width="28" height="24">
      <path fill-rule="evenodd" clip-rule="evenodd"
          d="M6 5C6 4.44772 6.44772 4 7 4H29C29.5523 4 30 4.44772 30 5V23C30 23.5523 29.5523 24 29 24H7C6.44772 24 6 23.5523 6 23V5ZM7.99992 22V6.33335H17.9999V12H27.9999V22H7.99992ZM2 9C2 8.44772 2.44772 8 3 8H4V26H26V27C26 27.5523 25.5523 28 25 28H3C2.44772 28 2 27.5523 2 27V9Z"
          fill="#616161">
    </mask>
    <g mask="url(#mask0)">
      <rect width="32" height="32" fill="#5F6368">
    </g>
  </svg>
</div>
<div class="text-container" aria-hidden="true">
  <div id="primaryText" title="${this.data.tabGroup.title}"></div>
  <div id="secondaryTextContainer">
    <svg id="groupSvg" viewBox="-5 -5 10 10" xmlns="http://www.w3.org/2000/svg">
      <circle id= "groupDot" cx="0" cy="0" r="4">
    </svg>
    <div id="secondaryText">${this.tabCountText_()}</div>
    <div class="separator">•</div>
    <div id="timestamp">${this.data.tabGroup.lastActiveElapsedText}</div>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
