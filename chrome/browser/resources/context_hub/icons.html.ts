// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from '//resources/js/static_types.js';

const div = document.createElement('div');
div.innerHTML = getTrustedHTML`
<cr-iconset name="context-hub" size="24">
  <svg>
    <defs>
      <!-- Source: Material Symbols topic -->
      <g id="topic" viewBox="0 -960 960 960">
        <path d="M160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h240
            l80 80h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160
            Zm0-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Zm240-120h320v-80
            H400v80Zm0-120h320v-80H400v80Z">
        </path>
      </g>
      <!-- Source: Material Symbols folder -->
      <g id="folder" viewBox="0 -960 960 960">
        <path d="M160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h240
            l80 80h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160
            Zm0-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Z">
        </path>
      </g>
      <!-- Source: Material Symbols sell / local_offer -->
      <g id="tag" viewBox="0 0 24 24">
        <path d="M21.41 11.58l-9-9C12.05 2.22 11.55 2 11 2H4c-1.1 0-2 .9-2 2v7
            c0 .55.22 1.05.59 1.42l9 9c.36.36.86.58 1.41.58.55 0 1.05-.22
            1.41-.59l7-7c.37-.36.59-.86.59-1.41 0-.55-.23-1.06-.59-1.42zM13
            20.01L4 11V4h7v-.01l9 9-7 7.02z M6.5 8C5.67 8 5 7.33 5 6.5S5.67 5
            6.5 5 8 5.67 8 6.5 7.33 8 6.5 8z">
        </path>
      </g>
      <!-- Source: Material Symbols table_rows / view_list -->
      <g id="table-view" viewBox="0 -960 960 960">
        <path d="M120-240v-80h720v80H120Zm0-200v-80h720v80H120Zm0-200v-80h720v80
            H120Z">
        </path>
      </g>
      <!-- Source: Material Symbols grid_view -->
      <g id="grid-view" viewBox="0 -960 960 960">
        <path d="M120-520v-320h320v320H120Zm0 400v-320h320v320H120Zm400-400v-320
            h320v320H520Zm0 400v-320h320v320H520ZM200-600h160v-160H200v160Zm400
            0h160v-160H600v160Zm-400 400h160v-160H200v160Zm400 0h160v-160H600
            v160Z">
        </path>
      </g>
    </defs>
  </svg>
</cr-iconset>`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
