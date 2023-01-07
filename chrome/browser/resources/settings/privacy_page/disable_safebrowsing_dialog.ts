// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'disable-safebrowsing-dialog' makes sure users want to disable safebrowsing.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './disable_safebrowsing_dialog.html.js';

export interface SettingsDisableSafebrowsingDialogElement {
  $: {
    cancel: CrButtonElement,
    confirm: CrButtonElement,
    dialog: CrDialogElement,
  };
}

export class SettingsDisableSafebrowsingDialogElement extends PolymerElement {
  static get is() {
    return 'settings-disable-safebrowsing-dialog';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  /** @return Whether the user confirmed the dialog. */
  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  private onDialogCancel_() {
    this.$.dialog.cancel();
  }

  private onDialogConfirm_() {
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-disable-safebrowsing-dialog':
        SettingsDisableSafebrowsingDialogElement;
  }
}

customElements.define(
    SettingsDisableSafebrowsingDialogElement.is,
    SettingsDisableSafebrowsingDialogElement);
