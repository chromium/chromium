// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_bookmarks_add_folder_button.html.js';

export class PowerBookmarksAddFolderButtonElement extends PolymerElement {
  static get is() {
    return 'power-bookmarks-add-folder-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      compact: {
        type: Boolean,
        value: false,
      },
    };
  }

  declare disabled: boolean;
  declare compact: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-add-folder-button': PowerBookmarksAddFolderButtonElement;
  }
}

customElements.define(
    PowerBookmarksAddFolderButtonElement.is,
    PowerBookmarksAddFolderButtonElement);
