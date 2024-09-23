// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-dialog.js';
import './cra/cra-icon.js';

import {
  createRef,
  css,
  html,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';

import {CraDialog} from './cra/cra-dialog.js';

export class SystemAudioConsentDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: contents;
    }

    cra-dialog {
      width: 440px;

      & > [slot="headline"] {
        align-items: flex-start;
        display: flex;
        flex-direction: column;
        gap: 20px;
        justify-content: start;
      }

      cra-icon {
        color: var(--cros-sys-primary);
        height: 32px;
        margin: 0;
        width: 32px;
      }
    }
  `;

  private readonly dialog = createRef<CraDialog>();

  show(): void {
    void this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.close();
  }

  private onConsent() {
    this.hide();
    this.dispatchEvent(new CustomEvent('system-audio-consent-clicked'));
  }

  override render(): RenderResult {
    return html`
    <cra-dialog ${ref(this.dialog)}>
        <div slot="headline">
          <cra-icon name="laptop_chromebook"></cra-icon>
          ${i18n.systemAudioConsentDialogHeader}
        </div>
        <div slot="content">
          ${i18n.systemAudioConsentDialogDescription}
        </div>
        <div slot="actions">
          <cra-button
            .label=${i18n.systemAudioConsentDialogCancelButton}
            button-style="secondary"
            @click=${this.hide}
          >
          </cra-button>
          <cra-button
            .label=${i18n.systemAudioConsentDialogConsentButton}
            button-style="primary"
            @click=${this.onConsent}
          >
          </cra-button>
        </div>
    </cra-dialog>`;
  }
}

window.customElements.define(
  'system-audio-consent-dialog',
  SystemAudioConsentDialog,
);

declare global {
  interface HTMLElementTagNameMap {
    'system-audio-consent-dialog': SystemAudioConsentDialog;
  }
}
