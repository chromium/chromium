// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './power_bookmarks_add_folder_button.css.js';
import {getHtml} from './power_bookmarks_add_folder_button.html.js';

export class PowerBookmarksAddFolderButtonElement extends CrLitElement {
  static get is() {
    return 'power-bookmarks-add-folder-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
      compact: {type: Boolean},
    };
  }

  accessor disabled: boolean = false;
  accessor compact: boolean = false;
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-add-folder-button': PowerBookmarksAddFolderButtonElement;
  }
}

customElements.define(
    PowerBookmarksAddFolderButtonElement.is,
    PowerBookmarksAddFolderButtonElement);
