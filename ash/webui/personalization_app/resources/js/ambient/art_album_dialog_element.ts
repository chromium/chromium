// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying information for art albums.
 */

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './art_album_dialog_element.html.js';

export interface ArtAlbumDialog {
  $: {dialog: CrDialogElement};
}

export class ArtAlbumDialog extends WithPersonalizationStore {
  static get is() {
    return 'art-album-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private onClose_() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'art-album-dialog': ArtAlbumDialog;
  }
}

customElements.define(ArtAlbumDialog.is, ArtAlbumDialog);
