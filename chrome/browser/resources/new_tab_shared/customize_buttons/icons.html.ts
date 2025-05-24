// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`
<cr-iconset name="ntp">
  <svg>
    <defs>
      <g id="pencil" width="24" height="24">
        <path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a.996.996 0 0 0 0-1.41l-2.34-2.34a.996.996 0 0 0-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/><path d="M0 0h24v24H0z" fill="none"/>
      </g>
      <clipPath id="a"><path fill="#fff" d="M0 0h16v16H0z"/></clipPath>
      <g id="sparkle" clip-path="url(#a)" width="16" height="16" fill="none" viewBox="0 0 16 16">
        <path fill="#fff" d="M8 14.4a6.16 6.16 0 0 0-.5-2.483 6.286 6.286 0 0 0-1.383-2.034A6.287 6.287 0 0 0 4.083 8.5 6.16 6.16 0 0 0 1.6 8a6.16 6.16 0 0 0 2.483-.5A6.421 6.421 0 0 0 7.5 4.083c.333-.777.5-1.605.5-2.483 0 .878.167 1.706.5 2.483a6.623 6.623 0 0 0 1.367 2.05 6.624 6.624 0 0 0 2.05 1.367c.777.333 1.605.5 2.483.5-.878 0-1.706.167-2.483.5A6.421 6.421 0 0 0 8.5 11.917 6.16 6.16 0 0 0 8 14.4Z"/>
      </g>
    </defs>
  </svg>
</cr-iconset>`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
