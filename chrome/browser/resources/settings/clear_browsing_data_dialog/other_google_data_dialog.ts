// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-other-google-data-dialog' is a subpage
 * shown within the Clear Browsing Data dialog to provide links
 * for managing other Google data like passwords and activity.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './other_google_data_dialog.html.js';

export interface SettingsOtherGoogleDataDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class SettingsOtherGoogleDataDialogElement extends PolymerElement {
  static get is() {
    return 'settings-other-google-data-dialog';
  }

  static get template() {
    return getTemplate();
  }

  private onBackOrCancelClick_() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-other-google-data-dialog': SettingsOtherGoogleDataDialogElement;
  }
}

customElements.define(
    SettingsOtherGoogleDataDialogElement.is,
    SettingsOtherGoogleDataDialogElement);
