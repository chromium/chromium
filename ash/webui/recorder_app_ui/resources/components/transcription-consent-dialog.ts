// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../components/cra/cra-button.js';
import '../components/cra/cra-image.js';

import {createRef, css, html, ref} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {settings, TranscriptionEnableState} from '../core/state/settings.js';

import {CraDialog} from './cra/cra-dialog.js';

/**
 * Dialog for asking transcription consent from user.
 *
 * Note that this is different from onboarding dialog and is only used when
 * user defers transcription consent on onboarding, and then enable it later.
 *
 * The main difference to the onboarding dialog is that onboarding dialog is
 * not dismissable from user by clicking outside / pressing ESC, but this
 * dialog is, so this dialog can use cra-dialog as underlying implementation and
 * the onboarding dialog needs to be implemented manually, makes it hard for
 * these two to share implementation.
 *
 * TODO(pihsun): Consider other way to share part of the implementation.
 * TODO(pihsun): Extract the "dialog with illustration" for speaker ID consent.
 */
export class TranscriptionConsentDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    #header {
      align-items: stretch;
      display: flex;
      flex-flow: column;
      gap: 0;
      overflow: hidden;
      padding: 0;

      & > span {
        padding: 32px 32px 0;
      }
    }

    #illust {
      align-items: center;
      background: var(--cros-sys-highlight_shape);

      /*
       * Due to how the md-dialog put the box-shadow inside the container
       * element instead of on the container element, having overflow: hidden on
       * the whole dialog will also hide the box shadow, so a separate top
       * rounded border is needed here to hide extra background.
       */
      border-radius: 20px 20px 0 0;
      display: flex;
      height: 236px;
      justify-content: center;
    }

    .left {
      margin-right: auto;
    }
  `;

  private readonly dialog = createRef<CraDialog>();

  private readonly platformHandler = usePlatformHandler();

  async show(): Promise<void> {
    await this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.close();
  }

  private disableTranscription() {
    settings.mutate((s) => {
      s.transcriptionEnabled = TranscriptionEnableState.DISABLED_FIRST;
    });
    this.hide();
  }

  private enableTranscription() {
    // TODO: b/344787880 - This should show the speaker ID consent afterwards.
    settings.mutate((s) => {
      s.transcriptionEnabled = TranscriptionEnableState.ENABLED;
    });
    this.platformHandler.installSoda();
    this.hide();
  }

  override render(): RenderResult {
    return html`<cra-dialog ${ref(this.dialog)}>
      <div slot="headline" id="header">
        <div id="illust">
          <cra-image name="onboarding_transcription"></cra-image>
        </div>
        <span>${i18n.onboardingDialogTranscriptionHeader}</span>
      </div>
      <div slot="content">${i18n.onboardingDialogTranscriptionDescription}</div>
      <div slot="actions">
        <cra-button
          .label=${i18n.onboardingDialogTranscriptionDeferButton}
          class="left"
          button-style="secondary"
          @click=${this.hide}
        ></cra-button>
        <cra-button
          .label=${i18n.onboardingDialogTranscriptionCancelButton}
          button-style="secondary"
          @click=${this.disableTranscription}
        ></cra-button>
        <cra-button
          .label=${i18n.onboardingDialogTranscriptionTurnOnButton}
          @click=${this.enableTranscription}
        ></cra-button>
      </div>
    </cra-dialog>`;
  }
}

window.customElements.define(
  'transcription-consent-dialog',
  TranscriptionConsentDialog,
);

declare global {
  interface HTMLElementTagNameMap {
    'transcription-consent-dialog': TranscriptionConsentDialog;
  }
}
