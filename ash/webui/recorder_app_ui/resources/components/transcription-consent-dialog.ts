// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-feature-tour-dialog.js';
import './speaker-label-consent-dialog.js';

import {createRef, css, html, ref} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {settings, TranscriptionEnableState} from '../core/state/settings.js';

import {CraFeatureTourDialog} from './cra/cra-feature-tour-dialog.js';
import {SpeakerLabelConsentDialog} from './speaker-label-consent-dialog.js';

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
 */
export class TranscriptionConsentDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: contents;
    }

    .left {
      margin-right: auto;
    }

    cra-feature-tour-dialog {
      height: 512px;
    }
  `;

  private readonly dialog = createRef<CraFeatureTourDialog>();

  private readonly speakerLabelConsentDialog =
    createRef<SpeakerLabelConsentDialog>();

  private readonly platformHandler = usePlatformHandler();

  async show(): Promise<void> {
    await this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.hide();
  }

  private disableTranscription() {
    settings.mutate((s) => {
      s.transcriptionEnabled = TranscriptionEnableState.DISABLED_FIRST;
    });
    this.hide();
  }

  private enableTranscription() {
    settings.mutate((s) => {
      s.transcriptionEnabled = TranscriptionEnableState.ENABLED;
    });
    this.platformHandler.installSoda();
    if (this.platformHandler.canUseSpeakerLabel.value) {
      this.speakerLabelConsentDialog.value?.show();
    }
    this.hide();
  }

  override render(): RenderResult {
    // TODO(pihsun): The dialogs (like speaker-label-consent-dialog) are
    // currently initialized at multiple places when it needs to be used,
    // consider making it "global" so it'll only be rendered once?
    return html`<cra-feature-tour-dialog
        ${ref(this.dialog)}
        illustrationName="onboarding_transcription"
        header=${i18n.onboardingDialogTranscriptionHeader}
      >
        <div slot="content">
          ${i18n.onboardingDialogTranscriptionDescription}
        </div>
        <div slot="actions">
          <cra-button
            .label=${i18n.onboardingDialogTranscriptionDeferButton}
            class="left"
            @click=${this.hide}
          ></cra-button>
          <cra-button
            .label=${i18n.onboardingDialogTranscriptionCancelButton}
            @click=${this.disableTranscription}
          ></cra-button>
          <cra-button
            .label=${i18n.onboardingDialogTranscriptionTurnOnButton}
            @click=${this.enableTranscription}
          ></cra-button>
        </div>
      </cra-feature-tour-dialog>
      <speaker-label-consent-dialog ${ref(this.speakerLabelConsentDialog)}>
      </speaker-label-consent-dialog>`;
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
