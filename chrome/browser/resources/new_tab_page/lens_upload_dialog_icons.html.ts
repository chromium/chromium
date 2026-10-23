// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

// Illustrations used by <ntp-lens-upload-dialog>. Defined inline, rather than
// referenced by URL, so that they render regardless of the host document's base
// URL (e.g. when the element is reused outside of chrome://new-tab-page).
const div = document.createElement('div');
div.innerHTML = getTrustedHTML`
<cr-iconset name="ntp-lens-upload-dialog">
  <svg>
    <defs>
      <g id="drag-drop-illustration" viewBox="0 0 59 45">
        <path d="M40.333 13.747H1.583v29.708h38.75V13.747Z" fill="#F1F3F4"/>
        <path d="M40.333 13.747h-23.25v19.375h23.25V13.747Z" fill="#CCC"/>
        <path fill-rule="evenodd" clip-rule="evenodd" d="M.614 12.778H6.75v1.938H2.552v4.198H.614v-6.136Zm38.75 29.709v-4.198h1.938v6.135h-6.136v-1.937h4.198ZM.614 38.289h1.938v4.198H6.75v1.937H.614V38.29Z" fill="#BDC1C6"/>
        <path d="M19.666 30.253h38.75V.545h-38.75v29.708Z" fill="#AECBFA"/>
        <path d="M19.666 21.843v8.41h38.75V19.74l-8.75-7.334a5.625 5.625 0 0 0-3.739-1.305 5.62 5.62 0 0 0-3.673 1.478l-8.837 8.212-5-3.537a3.565 3.565 0 0 0-4.55.396l-4.2 4.192Z" fill="#669DF6"/>
        <path d="M30.006 12.939a3.125 3.125 0 1 0 0-6.25 3.125 3.125 0 0 0 0 6.25Z" fill="#E8F0FE"/>
      </g>
      <g id="offline" viewBox="0 0 55 51">
        <path d="m23.184 11.959-5.922-6.155C30.291 2.11 44.846 5.452 55 16.179l-4.908 5.1c-7.446-7.562-17.43-10.727-26.907-9.32Zm21.831 14.596c-4.23-4.396-9.477-6.858-15.061-7.386l11 11.431 4.061-4.045ZM4.57 0 1.015 3.693 7.277 10.2C4.738 11.783 2.2 13.717 0 16.004l4.908 5.1c2.2-2.287 4.738-4.221 7.446-5.628l5.246 5.452c-2.877 1.23-5.415 2.99-7.785 5.275l4.908 5.1c2.37-2.462 5.246-4.044 8.292-4.748l6.939 7.21c-3.554-1.054-7.446 0-10.154 2.814l7.446 7.738 6.43-6.682L46.539 51l3.554-3.693L4.57 0Z" fill="#BDC1C6"/>
      </g>
    </defs>
  </svg>
</cr-iconset>`;

const iconsets = div.querySelectorAll('cr-iconset');
for (const iconset of iconsets) {
  document.head.appendChild(iconset);
}
