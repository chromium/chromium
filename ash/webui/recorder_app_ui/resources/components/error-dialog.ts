// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './unescapable-dialog.js';

import {
  createRef,
  css,
  html,
  PropertyDeclarations,
  PropertyValues,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {assertExists} from '../core/utils/assert.js';

import {CraButton} from './cra/cra-button.js';

/**
 * A dialog that displays error message. Users must click the consent button to
 * close the dialog.
 */
export class ErrorDialog extends ReactiveLitElement {
  static override styles = css`
    .host {
      display: contents;
    }

    unescapable-dialog {
      width: 440px;

      /* From CrOS dialog style. Min width for Recorder App is 480px. */
      @media (width < 520px) {
        width: 360px;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    header: {type: String},
    open: {type: Boolean},
  };

  header = '';

  /**
   * Whether the dialog is opened.
   */
  open = false;

  private readonly closeButton = createRef<CraButton>();

  private close() {
    this.dispatchEvent(new Event('close'));
  }

  override updated(changedProperties: PropertyValues<this>): void {
    if (changedProperties.has('open') && this.open) {
      const closeButton = assertExists(this.closeButton.value);
      closeButton.updateComplete.then(() => {
        closeButton.focus();
      });
    }
  }

  override render(): RenderResult {
    return html`
      <unescapable-dialog
        header=${this.header}
        ?open=${this.open}
      >
        <div slot="description"><slot></slot></div>
        <div slot="actions">
          <cra-button
            .label=${i18n.errorDialogConsentButton}
            @click=${this.close}
            ${ref(this.closeButton)}
          ></cra-button>
        </div>
      </unescapable-dialog>
    `;
  }
}

window.customElements.define('error-dialog', ErrorDialog);

declare global {
  interface HTMLElementTagNameMap {
    'error-dialog': ErrorDialog;
  }
}
