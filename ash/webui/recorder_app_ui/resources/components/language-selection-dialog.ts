// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './language-dropdown.js';
import './cra/cra-feature-tour-dialog.js';

import {createRef, css, html, ref} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {
  setTranscriptionLanguage,
} from '../core/state/transcription.js';
import {assertExists} from '../core/utils/assert.js';

import {CraFeatureTourDialog} from './cra/cra-feature-tour-dialog.js';
import {LanguageDropdown} from './language-dropdown.js';
import {SpeakerLabelConsentDialog} from './speaker-label-consent-dialog.js';

/**
 * Dialog for selecting transcript language when onboarding.
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
 * TODO(hsuanling): Consider other way to share part of the implementation.
 */
export class LanguageSelectionDialog extends ReactiveLitElement {
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

    language-dropdown {
      margin-top: 16px;
    }
  `;

  private readonly dialog = createRef<CraFeatureTourDialog>();

  private readonly dropdown = createRef<LanguageDropdown>();

  private readonly speakerLabelConsentDialog =
    createRef<SpeakerLabelConsentDialog>();

  private readonly platformHandler = usePlatformHandler();

  async show(): Promise<void> {
    await this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.hide();
  }

  private cancelSelection() {
    this.hide();
  }

  private downloadLanguage() {
    const languageCode = assertExists(this.dropdown.value).value;
    if (languageCode === null) {
      return;
    }
    setTranscriptionLanguage(languageCode);
    if (this.platformHandler.canUseSpeakerLabel.value) {
      this.speakerLabelConsentDialog.value?.show();
    }
    this.hide();
  }

  override render(): RenderResult {
    // TODO(hsuanling): The dialogs (like speaker-label-consent-dialog) are
    // currently initialized at multiple places when it needs to be used,
    // consider making it "global" so it'll only be rendered once?
    return html`<cra-feature-tour-dialog
        ${ref(this.dialog)}
        illustrationName="onboarding_transcription"
        header=${i18n.onboardingDialogLanguageSelectionHeader}
      >
        <div slot="content">
          ${i18n.onboardingDialogLanguageSelectionDescription}
          <language-dropdown
            .languageList=${this.platformHandler.getLangPackList()}
            ${ref(this.dropdown)}
          >
          </language-dropdown>
        </div>
        <div slot="actions">
            <cra-button
              .label=${i18n.onboardingDialogLanguageSelectionCancelButton}
              @click=${this.cancelSelection}
            ></cra-button>
            <cra-button
              label=${i18n.onboardingDialogLanguageSelectionDownloadButton}
              @click=${this.downloadLanguage}
            ></cra-button>
        </div>
      </cra-feature-tour-dialog>
      <speaker-label-consent-dialog ${ref(this.speakerLabelConsentDialog)}>
      </speaker-label-consent-dialog>`;
  }
}

window.customElements.define(
  'language-selection-dialog',
  LanguageSelectionDialog,
);

declare global {
  interface HTMLElementTagNameMap {
    'language-selection-dialog': LanguageSelectionDialog;
  }
}
