// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-feature-tour-dialog.js';
import './language-selection-dialog.js';
import './speaker-label-consent-dialog.js';

import {createRef, css, html, ref} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {
  disableTranscription,
  enableTranscriptionSkipConsentCheck,
} from '../core/state/transcription.js';

import {CraFeatureTourDialog} from './cra/cra-feature-tour-dialog.js';
import {LanguageSelectionDialog} from './language-selection-dialog.js';
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

  private readonly languageSelectionDialog =
    createRef<LanguageSelectionDialog>();

  private readonly speakerLabelConsentDialog =
    createRef<SpeakerLabelConsentDialog>();

  private readonly platformHandler = usePlatformHandler();

  private readonly shouldShowSelector =
    this.platformHandler.isMultipleLanguageAvailable();

  async show(): Promise<void> {
    await this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.hide();
  }

  private disableTranscription() {
    disableTranscription(/* firstTime= */ true);
    this.hide();
  }

  private enableTranscription() {
    enableTranscriptionSkipConsentCheck();
    if (!this.shouldShowSelector) {
      if (this.platformHandler.canUseSpeakerLabel.value) {
        this.speakerLabelConsentDialog.value?.show();
      }
    } else {
      this.languageSelectionDialog.value?.show();
    }
    this.hide();
  }

  override render(): RenderResult {
    // TODO(pihsun): The dialogs (like speaker-label-consent-dialog) are
    // currently initialized at multiple places when it needs to be used,
    // consider making it "global" so it'll only be rendered once?
    const header = this.shouldShowSelector ?
      i18n.onboardingDialogTranscriptionTurnOnHeader :
      i18n.onboardingDialogTranscriptionHeader;
    const description = this.shouldShowSelector ?
      i18n.onboardingDialogTranscriptionTurnOnDescription :
      i18n.onboardingDialogTranscriptionDescription;
    const turnOnButtonLabel = this.shouldShowSelector ?
      i18n.onboardingDialogTranscriptionTurnOnButton :
      i18n.onboardingDialogTranscriptionDownloadButton;

    return html`<cra-feature-tour-dialog
        ${ref(this.dialog)}
        illustrationName="onboarding_transcription"
        header=${header}
      >
        <div slot="content">${description}</div>
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
            .label=${turnOnButtonLabel}
            @click=${this.enableTranscription}
          ></cra-button>
        </div>
      </cra-feature-tour-dialog>
      <language-selection-dialog ${ref(this.languageSelectionDialog)}>
      </language-selection-dialog>
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
