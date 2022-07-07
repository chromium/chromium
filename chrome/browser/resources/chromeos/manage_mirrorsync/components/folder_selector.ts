// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTemplate} from './folder_selector.html.js';

/**
 * FolderSelector presents a folder hierarchy of checkboxes representing the
 * underlying folder structure. The items are lazily loaded as required.
 */
class FolderSelector extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({mode: 'open'})
        .appendChild(getTemplate().content.cloneNode(true));
  }
}

customElements.define('folder-selector', FolderSelector);
