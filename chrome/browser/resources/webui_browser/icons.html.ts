// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from '//resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`
<cr-iconset name="webui-browser">
  <svg>
    <defs>
      <g id="folder"><path d="M4 20c-.55 0-1.02-.195-1.414-.586A1.935 1.935 0 0 1 2 18V6c0-.55.195-1.02.586-1.414C2.98 4.196 3.449 4 4 4h6l2 2h8c.55 0 1.02.195 1.414.586.39.394.586.863.586 1.414v10c0 .55-.195 1.02-.586 1.414-.394.39-.863.586-1.414.586Zm0-2h16V8h-8.824l-2-2H4Zm0 0V6Zm0 0"/></g>
      <g id="minimize"><path d="M 5 12 L 19 12" /></g>
      <g id="maximize"><rect x="5" y="5" width="14" height="14" /></g>
      <g id="restore">
        <rect x="7" y="3" width="14" height="14" rx="2" />
        <path d="M3 7h14" />
        <path d="M3 7v14a2 2 0 0 0 2 2h14" fill="none"/>
      </g>
      <g id="close">
        <line x1="18" y1="6" x2="6" y2="18" />
        <line x1="6" y1="6" x2="18" y2="18" />
      </g>
    </defs>
  </svg>
</cr-iconset>`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
