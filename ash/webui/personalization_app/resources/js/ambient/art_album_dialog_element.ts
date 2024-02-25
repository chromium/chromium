// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying information for art albums.
 */

import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './art_album_dialog_element.html.js';

export interface ArtAlbumDialogElement {
  $: {dialog: CrDialogElement};
}

export class ArtAlbumDialogElement extends WithPersonalizationStore {
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
    'art-album-dialog': ArtAlbumDialogElement;
  }
}

customElements.define(ArtAlbumDialogElement.is, ArtAlbumDialogElement);
