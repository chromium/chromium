// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying information for art albums.
 */

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WithPersonalizationStore} from '../personalization_store.js';

export interface ArtAlbumDialog {
  $: {dialog: CrDialogElement}
}

export class ArtAlbumDialog extends WithPersonalizationStore {
  static get is() {
    return 'art-album-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
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
