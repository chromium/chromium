// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Displays a dialog informing the user that a Google Photos album
 * selected for daily refresh is shared with other Google Photos accounts.
 */

import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isGooglePhotosSharedAlbumsEnabled} from '../load_time_booleans.js';

import {getTemplate} from './google_photos_shared_album_dialog_element.html.js';

export class AcceptEvent extends CustomEvent<null> {
  static readonly EVENT_NAME = 'shared-album-dialog-accept';

  constructor() {
    super(
        AcceptEvent.EVENT_NAME,
        {
          bubbles: true,
          composed: true,
          detail: null,
        },
    );
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'google-photos-shared-album-dialog': GooglePhotosSharedAlbumDialogElement;
  }
}

export interface GooglePhotosSharedAlbumDialogElement {
  $: {dialog: CrDialogElement};
}

export class GooglePhotosSharedAlbumDialogElement extends PolymerElement {
  static get is() {
    return 'google-photos-shared-album-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  override connectedCallback(): void {
    assert(
        isGooglePhotosSharedAlbumsEnabled(),
        'google photos shared albums must be enabled');
    super.connectedCallback();
  }

  private onClickAccept_() {
    this.dispatchEvent(new AcceptEvent());
  }

  private onClickClose_() {
    this.$.dialog.cancel();
  }
}

customElements.define(
    GooglePhotosSharedAlbumDialogElement.is,
    GooglePhotosSharedAlbumDialogElement);
