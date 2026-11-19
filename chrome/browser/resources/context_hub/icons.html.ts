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
      <!-- Source: Material Icons auto_awesome -->
      <g id="auto-awesome" viewBox="0 0 24 24">
        <path d="M19 9l1.25-2.75L23 5l-2.75-1.25L19 1l-1.25 2.75L15 5l2.75
            1.25L19 9zm-7.5.5L9 4 6.5 9.5 1 12l5.5 2.5L9 20l2.5-5.5L17
            12l-5.5-2.5zM19 15l-1.25 2.75L15 19l2.75 1.25L19 23l1.25-2.75L23
            19l-2.75-1.25L19 15z">
        </path>
      </g>
      <!-- Source: Material Symbols radio_button_unchecked -->
      <g id="radio-button-unchecked" viewBox="0 -960 960 960">
        <path d="M480-80q-83 0-156-31.5T197-197q-54-54-85.5-127T80-480q0-83
            31.5-156T197-763q54-54 127-85.5T480-880q83 0 156 31.5T763-763q54
            54 85.5 127T880-480q0 83-31.5 156T763-197q-54 54-127 85.5T480-80
            Zm0-80q134 0 227-93t93-227q0-134-93-227t-227-93q-134 0-227 93t-93
            227q0 134 93 227t227 93Z">
        </path>
      </g>
      <!-- Source: cursor_spark -->
      <g id="cursor-spark" viewBox="0 0 20 20">
        <path d="m 5.58 15.9 l 3.75 -3.37 h 5.12 L 6.25 4.77 Z M 4 16.86 l 0.75
            -12.61 c 0.02 -0.27 0.12 -0.52 0.29 -0.73 c 0.16 -0.21 0.35 -0.35
            0.56 -0.44 c 0.21 -0.08 0.44 -0.1 0.7 -0.07 c 0.26 0.04 0.48 0.15
            0.68 0.34 l 9.29 8.81 c 0.18 0.17 0.3 0.36 0.36 0.58 c 0.06 0.22
            0.05 0.42 -0.02 0.62 c -0.07 0.19 -0.21 0.36 -0.42 0.5 c -0.21 0.14
            -0.43 0.21 -0.67 0.21 H 9.92 L 5.83 17.73 a 1.04 1.04 0 0 1 -0.6
            0.27 a 1.16 1.16 0 0 1 -0.62 -0.08 a 1.02 1.02 0 0 1 -0.45 -0.42
            A 1.09 1.09 0 0 1 4 16.86 Z M 15.5 8 c -0.08 0 -0.14 -0.04 -0.17
            -0.12 a 4.24 4.24 0 0 0 -1.16 -2.05 a 4.33 4.33 0 0 0 -2.06 -1.16
            C 12.04 4.64 12 4.58 12 4.5 c 0 -0.1 0.04 -0.15 0.13 -0.17 a 4.24
            4.24 0 0 0 2.05 -1.16 a 4.33 4.33 0 0 0 1.17 -2.06 c 0.02 -0.08
            0.08 -0.12 0.17 -0.12 c 0.09 0 0.14 0.04 0.16 0.13 a 4.39 4.39 0 0
            0 1.17 2.04 a 4.39 4.39 0 0 0 2.04 1.17 c 0.08 0.02 0.13 0.07 0.13
            0.17 c 0 0.08 -0.04 0.14 -0.12 0.17 a 4.24 4.24 0 0 0 -2.05 1.16
            a 4.29 4.29 0 0 0 -1.16 2.06 c -0.01 0.08 -0.07 0.12 -0.16 0.12 Z
            m -6.17 4.52 Z">
        </path>
      </g>
    </defs>
  </svg>
</cr-iconset>`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
