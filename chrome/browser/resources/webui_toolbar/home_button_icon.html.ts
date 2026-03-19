// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from '//resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`
<cr-iconset name="home-button" size="20">
  <svg>
    <defs>
      <g id="navigate-home" fill-rule="nonzero">
        <path fill="currentColor" d="M 5.5 15.5 h 2 v -5 h 5 v 5 h 2 V 8.25 L 10
4.88 L 5.5 8.25 Z M 4 17 V 7.5 L 10 3 l 6 4.5 V 17 h -5 v -5 H 9 v 5 Zm 6 -6.81
Z"/>
      </g>
    </defs>
  </svg>
</cr-iconset>
`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
