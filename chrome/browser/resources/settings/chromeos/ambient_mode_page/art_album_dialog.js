// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information for art albums.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ArtAlbumDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class ArtAlbumDialogElement extends ArtAlbumDialogElementBase {
  static get is() {
    return 'art-album-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  /**
   * Closes the dialog.
   * @private
   */
  onClose_() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }
}

customElements.define(ArtAlbumDialogElement.is, ArtAlbumDialogElement);
