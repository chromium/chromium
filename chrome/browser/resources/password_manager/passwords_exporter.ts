// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './passwords_exporter.html.js';

export interface PasswordsExporterElement {
  $: {
    exportPasswordsButton: HTMLElement,
  };
}

const PasswordsExporterElementBase = I18nMixin(PolymerElement);

export class PasswordsExporterElement extends PasswordsExporterElementBase {
  static get is() {
    return 'password-exporter';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Whether password export dialog is shown. */
      showPasswordsExportDialog_: Boolean,
    };
  }

  private showPasswordsExportDialog_: boolean;

  /**
   * Opens the export passwords dialog.
   */
  private onExportClick_() {
    this.showPasswordsExportDialog_ = true;
  }

  /**
   * Closes the export passwords dialog.
   */
  private onPasswordsExportDialogClosed_() {
    this.showPasswordsExportDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-exporter': PasswordsExporterElement;
  }
}

customElements.define(PasswordsExporterElement.is, PasswordsExporterElement);
