// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from '//resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`
<cr-iconset name="split-tabs-button" size="24">
  <svg>
    <defs>
      <g id="split-scene">
        <path fill="currentColor" d="M 5 20 C 4.45 20 3.98 19.8 3.59 19.41 C 3.2
19.02 3 18.55 3 18 L 3 6 C 3 5.45 3.2 4.98 3.59 4.59 C 3.98 4.2 4.45 4 5 4 L 9 4
L 9 6 L 5 6 L 5 18 L 9 18 L 9 20 Z M 11 22 L 11 2 L 13 2 L 13 4 L 19 4 C 19.55 4
20.02 4.2 20.41 4.59 C 20.8 4.98 21 5.45 21 6 L 21 18 C 21 18.55 20.8 19.02
20.41 19.41 C 20.02 19.8 19.55 20 19 20 L 13 20 L 13 22 Z M 13 18 L 19 18 L 19 6
L 13 6 Z">
        </path>
      </g>
      <g id="split-scene-left">
        <path fill="currentColor" d="M 15 20 L 15 18 L 19 18 L 19 6 L 15 6 L 15
4 L 19 4 C 19.55 4 20.02 4.2 20.41 4.59 C 20.8 4.98 21 5.45 21 6 L 21 18 C 21
18.55 20.8 19.02 20.41 19.41 C 20.02 19.8 19.55 20 19 20 Z M 11 22 L 11 20 L 5
20 C 4.45 20 3.98 19.8 3.59 19.41 C 3.2 19.02 3 18.55 3 18 L 3 6 C 3 5.45 3.2
4.98 3.59 4.59 C 3.98 4.2 4.45 4 5 4 L 11 4 L 11 2 L 13 2 L 13 22 Z M 19 6 L 19
18 Z M 19 6 Z">
        </path>
      </g>
      <g id="split-scene-right">
        <path fill="currentColor" d="M 5 20 C 4.45 20 3.98 19.8 3.59 19.41 C 3.2
19.02 3 18.55 3 18 L 3 6 C 3 5.45 3.2 4.98 3.59 4.59 C 3.98 4.2 4.45 4 5 4 L 9 4
L 9 6 L 5 6 L 5 18 L 9 18 L 9 20 Z M 11 22 L 11 2 L 13 2 L 13 4 L 19 4 C 19.55 4
20.02 4.2 20.41 4.59 C 20.8 4.98 21 5.45 21 6 L 21 18 C 21 18.55 20.8 19.02
20.41 19.41 C 20.02 19.8 19.55 20 19 20 L 13 20 L 13 22 Z M 5 18 L 5 6 Z M 5 18
Z">
        </path>
      </g>
      <g id="split-scene-up">
        <path fill="currentColor" d="M 6 21 C 5.45 21 4.98 20.8 4.59 20.41 C 4.2
20.02 4 19.55 4 19 L 4 15 L 6 15 L 6 19 L 18 19 L 18 15 L 20 15 L 20 19 C 20
19.55 19.8 20.02 19.41 20.41 C 19.02 20.8 18.55 21 18 21 Z M 2 13 L 2 11 L 4 11
L 4 5 C 4 4.45 4.2 3.98 4.59 3.59 C 4.98 3.2 5.45 3 6 3 L 18 3 C 18.55 3 19.02
3.2 19.41 3.59 C 19.8 3.98 20 4.45 20 5 L 20 11 L 22 11 L 22 13 Z M 18 19 L 6 19
Z M 18 19 Z">
        </path>
      </g>
      <g id="split-scene-down">
        <path fill="currentColor" d="M 4 9 L 4 5 C 4 4.45 4.2 3.98 4.59 3.59 C
          4.98 3.2 5.45 3 6 3 L 18 3 C 18.55 3 19.02 3.2 19.41 3.59 C 19.8 3.98
          20 4.45 20 5 L 20 9 L 18 9 L 18 5 L 6 5 L 6 9 Z M 6 21 C 5.45 21 4.98
          20.8 4.59 20.41 C 4.2 20.02 4 19.55 4 19 L 4 13 L 2 13 L 2 11 L 22 11
          L 22 13 L 20 13 L 20 19 C 20 19.55 19.8 20.02 19.41 20.41 C 19.02 20.8
          18.55 21 18 21 Z M 6 5 L 18 5 Z M 6 5 Z">
        </path>
      </g>
    </defs>
  </svg>
</cr-iconset>
`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
