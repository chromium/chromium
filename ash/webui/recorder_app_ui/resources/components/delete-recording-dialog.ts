// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-dialog.js';

import {
  createRef,
  css,
  CSSResultGroup,
  html,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';

import {CraDialog} from './cra/cra-dialog.js';

export class DeleteRecordingDialog extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    cra-dialog {
      width: 368px;
    }
  `;

  private readonly dialog = createRef<CraDialog>();

  async show(): Promise<void> {
    await this.dialog.value?.show();
  }

  private hide() {
    this.dialog.value?.close();
  }

  private emitDelete() {
    this.dispatchEvent(new CustomEvent('delete'));
    this.hide();
  }

  override render(): RenderResult {
    return html`<cra-dialog ${ref(this.dialog)}>
      <div slot="headline">${i18n.recordDeleteDialogHeader}</div>
      <div slot="actions">
        <cra-button
          .label=${i18n.recordDeleteDialogCancelButton}
          button-style="secondary"
          @click=${this.hide}
        ></cra-button>
        <cra-button
          .label=${i18n.recordDeleteDialogDeleteButton}
          @click=${this.emitDelete}
        ></cra-button>
      </div>
    </cra-dialog>`;
  }
}

window.customElements.define('delete-recording-dialog', DeleteRecordingDialog);

declare global {
  interface HTMLElementTagNameMap {
    'delete-recording-dialog': DeleteRecordingDialog;
  }
}
