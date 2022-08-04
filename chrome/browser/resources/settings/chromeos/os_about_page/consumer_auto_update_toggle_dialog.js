// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './consumer_auto_update_toggle_dialog.html.js';

/** @polymer */
class SettingsConsumerAutoUpdateToggleDialogElement extends PolymerElement {
  static get is() {
    return 'settings-consumer-auto-update-toggle-dialog';
  }

  static get template() {
    return getTemplate();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  /** @private */
  onTurnOffTap_() {
    this.dispatchEvent(new CustomEvent('set-consumer-auto-update', {
      bubbles: true,
      composed: true,
      detail: {
        item: false,
      },
    }));
    this.$.dialog.close();
  }

  /** @private */
  onKeepUpdatesTap_() {
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

customElements.define(
    SettingsConsumerAutoUpdateToggleDialogElement.is,
    SettingsConsumerAutoUpdateToggleDialogElement);
