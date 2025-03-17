// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-image.js';
import './language-dropdown.js';
import './speaker-label-consent-dialog-content.js';
import './unescapable-dialog.js';

import {
  createRef,
  css,
  html,
  keyed,
  PropertyDeclarations,
  PropertyValues,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n, NoArgStringName} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {computed, signal} from '../core/reactive/signal.js';
import {LanguageCode} from '../core/soda/language_info.js';
import {settings, SpeakerLabelEnableState} from '../core/state/settings.js';
import {
  disableTranscription,
  enableTranscriptionSkipConsentCheck,
  setTranscriptionLanguage,
} from '../core/state/transcription.js';
import {assertExhaustive} from '../core/utils/assert.js';

import {
  DESCRIPTION_NAMES as SPEAKER_LABEL_DIALOG_DESCRIPTION_NAMES,
} from './speaker-label-consent-dialog-content.js';

/**
 * A dialog for showing the onboarding flow.
 */
export class OnboardingDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    #dialog {
      height: 512px;

      /* Want at least 80px left/right margin. */
      width: min(512px, 100vw - 160px);

      /* From CrOS dialog style. Min width for Recorder App is 480px. */
      @media (width < 520px) {
        width: 360px;
      }
    }

    language-dropdown {
      margin-top: 16px;
    }

    .left {
      margin-right: auto;
    }
  `;

  static override properties: PropertyDeclarations = {
    open: {type: Boolean},
    step: {state: true},
  };

  /**
   * Whether the dialog is opened.
   */
  open = false;

  /**
   * The currently shown step index starting from 0.
   *
   * Step 0: Welcoming page.
   * Step 1: Transcription consent dialog for users to turn on transcription and
   * download en-US model.
   * Step 2: Transcription consent dialog for users to turn on transcription.
   * Step 3: Transcription language selection dialog for users to choose and
   * download language model.
   * Step 4: Speaker label consent dialog for users to turn on speaker label.
   *
   * When there are multiple language options, skip step 1. Otherwise, skip step
   * 2 and step 3.
   */
  step: 0|1|2|3|4 = 0;

  private readonly platformHandler = usePlatformHandler();

  private readonly autoFocusItem = createRef<ReactiveLitElement>();

  private readonly selectedLanguage = signal<LanguageCode>(
    this.platformHandler.getDefaultLanguage(),
  );

  private readonly availableLanguages = computed(() => {
    const languageList = this.platformHandler.getLangPackList();
    return languageList.filter((langPack) => {
      const sodaState =
        this.platformHandler.getSodaState(langPack.languageCode);
      return sodaState.value.kind !== 'unavailable';
    });
  });

  override updated(changedProperties: PropertyValues<this>): void {
    if (changedProperties.has('step') || changedProperties.has('open')) {
      const autoFocusItem = this.autoFocusItem.value;
      if (autoFocusItem !== undefined) {
        autoFocusItem.updateComplete.then(() => {
          autoFocusItem.focus();
        });
      }
    }
  }

  private sendOnboardEvent(): void {
    this.platformHandler.eventsSender.sendOnboardEvent({
      speakerLabelEnableState: settings.value.speakerLabelEnabled,
      transcriptionAvailable: this.platformHandler.isSodaAvailable(),
      transcriptionEnableState: settings.value.transcriptionEnabled,
    });
  }

  private close() {
    this.sendOnboardEvent();
    this.dispatchEvent(new Event('close'));
  }

  private renderDialog(
    step: number,
    imageName: string,
    header: string,
    description: RenderResult,
    buttons: RenderResult,
  ): RenderResult {
    // Force render a different element when step change, so ChromeVox would
    // convey the header of the new dialog.
    return keyed(
      step,
      html`<unescapable-dialog
        id="dialog"
        illustrationName=${imageName}
        header=${header}
        ?open=${this.open}
      >
        <div slot="description">${description}</div>
        <div id="buttons" slot="actions">${buttons}</div>
      </unescapable-dialog>`,
    );
  }

  override render(): RenderResult {
    // Note that all the onboarding_ images are currently placeholders and
    // don't use dynamic color tokens yet.
    // TODO: b/344785475 - Change to final illustration when ready.
    switch (this.step) {
      case 0: {
        const nextStep = () => {
          if (!this.platformHandler.isSodaAvailable()) {
            // SODA isn't available on this platform. Don't ask for enabling
            // transcription or speaker label.
            this.close();
            return;
          }
          this.step =
            this.platformHandler.isMultipleLanguageAvailable() ? 2 : 1;
        };
        return this.renderDialog(
          this.step,
          'onboarding_welcome',
          i18n.onboardingDialogWelcomeHeader,
          i18n.onboardingDialogWelcomeDescription,
          html`<cra-button
            .label=${i18n.onboardingDialogWelcomeNextButton}
            @click=${nextStep}
            ${ref(this.autoFocusItem)}
          ></cra-button>`,
        );
      }
      case 1: {
        const turnOnTranscription = () => {
          enableTranscriptionSkipConsentCheck();
          if (!this.platformHandler.canUseSpeakerLabel.value) {
            // Speaker label isn't supported on this platform.
            this.close();
            return;
          }
          this.step = 4;
        };
        const turnOffTranscription = () => {
          disableTranscription(/* firstTime= */ true);
          this.close();
        };
        return this.renderDialog(
          this.step,
          'onboarding_transcription',
          i18n.onboardingDialogTranscriptionHeader,
          i18n.onboardingDialogTranscriptionDescription,
          html`
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionDeferButton}
              class="left"
              @click=${this.close}
              ${ref(this.autoFocusItem)}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionCancelButton}
              @click=${turnOffTranscription}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionDownloadButton}
              @click=${turnOnTranscription}
            ></cra-button>
          `,
        );
      }
      case 2: {
        const turnOnTranscription = () => {
          enableTranscriptionSkipConsentCheck();
          this.step = 3;
        };
        const turnOffTranscription = () => {
          disableTranscription(/* firstTime= */ true);
          this.close();
        };
        return this.renderDialog(
          this.step,
          'onboarding_transcription',
          i18n.onboardingDialogTranscriptionTurnOnHeader,
          i18n.onboardingDialogTranscriptionTurnOnDescription,
          html`
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionDeferButton}
              class="left"
              @click=${this.close}
              ${ref(this.autoFocusItem)}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionCancelButton}
              @click=${turnOffTranscription}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionTurnOnButton}
              @click=${turnOnTranscription}
            ></cra-button>
          `,
        );
      }
      case 3: {
        const onDropdownChange = (ev: CustomEvent<LanguageCode>) => {
          this.selectedLanguage.value = ev.detail;
        };

        const dialogBody = html`
          ${i18n.onboardingDialogLanguageSelectionDescription}
          <language-dropdown
            .defaultLanguage=${this.platformHandler.getDefaultLanguage()}
            .languageList=${this.availableLanguages.value}
            @dropdown-changed=${onDropdownChange}
            ${ref(this.autoFocusItem)}
          >
          </language-dropdown>
        `;

        const downloadLanguage = () => {
          const languageCode = this.selectedLanguage.value;
          if (languageCode === null) {
            return;
          }
          setTranscriptionLanguage(languageCode);
          if (!this.platformHandler.canUseSpeakerLabel.value) {
            // Speaker label isn't supported on this platform.
            this.close();
            return;
          }
          this.step = 4;
        };

        const cancelSelection = () => {
          this.close();
        };

        return this.renderDialog(
          this.step,
          'onboarding_transcription',
          i18n.onboardingDialogLanguageSelectionHeader,
          dialogBody,
          html`
            <cra-button
              .label=${i18n.onboardingDialogLanguageSelectionCancelButton}
              @click=${cancelSelection}
            ></cra-button>
            <cra-button
              label=${i18n.onboardingDialogLanguageSelectionDownloadButton}
              @click=${downloadLanguage}
              .disabled=${this.selectedLanguage.value === null}
            ></cra-button>
          `,
        );
      }
      case 4: {
        const ALLOW_BUTTON_NAME: NoArgStringName =
          'onboardingDialogSpeakerLabelAllowButton';
        const DISALLOW_BUTTON_NAME: NoArgStringName =
          'onboardingDialogSpeakerLabelDisallowButton';
        const disableSpeakerLabel = () => {
          settings.mutate((s) => {
            s.speakerLabelEnabled = SpeakerLabelEnableState.DISABLED_FIRST;
          });
          this.platformHandler.recordSpeakerLabelConsent(
            false,
            SPEAKER_LABEL_DIALOG_DESCRIPTION_NAMES,
            DISALLOW_BUTTON_NAME,
          );
          this.close();
        };

        const enableSpeakerLabel = () => {
          settings.mutate((s) => {
            s.speakerLabelEnabled = SpeakerLabelEnableState.ENABLED;
          });
          this.platformHandler.recordSpeakerLabelConsent(
            true,
            SPEAKER_LABEL_DIALOG_DESCRIPTION_NAMES,
            ALLOW_BUTTON_NAME,
          );
          this.close();
        };

        return this.renderDialog(
          this.step,
          'onboarding_speaker_label',
          i18n.onboardingDialogSpeakerLabelHeader,
          html`<speaker-label-consent-dialog-content>
          </speaker-label-consent-dialog-content>`,
          html`
            <cra-button
              .label=${i18n.onboardingDialogSpeakerLabelDeferButton}
              class="left"
              @click=${this.close}
              ${ref(this.autoFocusItem)}
            ></cra-button>
            <cra-button
              .label=${i18n[DISALLOW_BUTTON_NAME]}
              @click=${disableSpeakerLabel}
            ></cra-button>
            <cra-button
              .label=${i18n[ALLOW_BUTTON_NAME]}
              @click=${enableSpeakerLabel}
            ></cra-button>
          `,
        );
      }
      default:
        assertExhaustive(this.step);
    }
  }
}

window.customElements.define('onboarding-dialog', OnboardingDialog);

declare global {
  interface HTMLElementTagNameMap {
    'onboarding-dialog': OnboardingDialog;
  }
}
