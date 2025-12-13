// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {BookmarkType} from './bookmark_bar.mojom-webui.js';
import type {BookmarkElement} from './bookmark_element.js';

export function getHtml(this: BookmarkElement) {
  return html`
  <div class="bookmark">
    <div id="faviconContainer">
      ${
      this.data.type === BookmarkType.FOLDER ?
          html`<cr-icon id="folderIcon" icon="webui-browser:folder"></cr-icon>` :
          html`<div id="favicon"></div>`}
    </div>
    <span class="bookmarkTitle">${this.data.title}</span>
  </div>
  `;
}
