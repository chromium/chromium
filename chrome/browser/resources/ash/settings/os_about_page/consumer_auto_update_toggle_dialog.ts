// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './consumer_auto_update_toggle_dialog.html.js';

export interface SettingsConsumerAutoUpdateToggleDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class SettingsConsumerAutoUpdateToggleDialogElement extends
    PolymerElement {
  static get is() {
    return 'settings-consumer-auto-update-toggle-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private onTurnOffClick_(): void {
    this.dispatchEvent(new CustomEvent('set-consumer-auto-update', {
      bubbles: true,
      composed: true,
      detail: {
        item: false,
      },
    }));
    this.$.dialog.close();
  }

  private onKeepUpdatesClick_(): void {
    this.dispatchEvent(new CustomEvent('set-consumer-auto-update', {
      bubbles: true,
      composed: true,
      detail: {
        item: true,
      },
    }));
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsConsumerAutoUpdateToggleDialogElement.is]:
        SettingsConsumerAutoUpdateToggleDialogElement;
  }
}

customElements.define(
    SettingsConsumerAutoUpdateToggleDialogElement.is,
    SettingsConsumerAutoUpdateToggleDialogElement);
