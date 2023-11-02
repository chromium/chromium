// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-confirmation-dialog' is a wrapper of
 * <cr-dialog> which
 *
 * - shows an accept button and a cancel button (you can customize the label via
 *   props);
 * - The close event has a boolean `e.detail.accepted` indicating whether the
 *   dialog is accepted or not.
 * - The dialog shows itself automatically when it is attached.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SettingsCrostiniConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'settings-crostini-confirmation-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      acceptButtonText: String,
      cancelButtonText: {
        type: String,
        value: loadTimeData.getString('cancel'),
      },
    };
  }

  constructor() {
    super();

    this.accepted_ = true;
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  }

  /** @private */
  onAcceptTap_() {
    this.$.dialog.close();
  }

  /** @private */
  onDialogCancel_(e) {
    this.accepted_ = false;
  }

  /** @private */
  onDialogClose_(e) {
    e.stopPropagation();

    const closeEvent = new CustomEvent(
        'close',
        {bubbles: true, composed: true, detail: {'accepted': this.accepted_}});
    this.dispatchEvent(closeEvent);
  }
}

customElements.define(
    SettingsCrostiniConfirmationDialogElement.is,
    SettingsCrostiniConfirmationDialogElement);
