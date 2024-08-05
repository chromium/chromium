// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-image.js';
import './speaker-label-consent-dialog-content.js';

import {
  css,
  html,
  PropertyDeclarations,
  PropertyValues,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {
  settings,
  SpeakerLabelEnableState,
  TranscriptionEnableState,
} from '../core/state/settings.js';
import {assertExhaustive, assertInstanceof} from '../core/utils/assert.js';

/**
 * A dialog for showing the onboarding flow.
 */
export class OnboardingDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    #dialog {
      background: var(--cros-sys-base_elevated);
      border: none;
      border-radius: 20px;
      box-shadow: var(--cros-sys-app_elevation3);
      color: var(--cros-sys-on_surface);
      display: flex;
      flex-flow: column;
      height: 512px;
      padding: 0;

      /* Want at least 80px left/right margin. */
      width: min(512px, 100vw - 160px);

      &::backdrop {
        background: var(--cros-sys-scrim);
        pointer-events: none;
      }

      /* From CrOS dialog style. Min width for Recorder App is 480px. */
      @media (width < 520px) {
        width: 360px;
      }
    }

    #illust {
      align-items: center;
      background: var(--cros-sys-highlight_shape);
      display: flex;
      height: 236px;
      justify-content: center;
      overflow: hidden;
      width: 100%;
    }

    #content {
      display: flex;
      flex: 1;
      flex-flow: column;
      min-height: 0;
      padding: 32px 32px 28px;
    }

    #header {
      font: var(--cros-display-7-font);
      margin-bottom: 16px;
    }

    #description {
      color: var(--cros-sys-on_surface_variant);
      flex: 1;
      font: var(--cros-body-1-font);
      margin-bottom: 32px;
      overflow-y: auto;
    }

    #buttons {
      display: flex;
      flex-flow: row;
      gap: 8px;
      justify-content: right;

      & > .left {
        margin-right: auto;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    open: {type: Boolean},
  };

  /**
   * Whether the dialog is opened.
   */
  open = false;

  /**
   * The currently shown step index starting from 0.
   */
  step = signal<0|1|2>(0);

  private readonly platformHandler = usePlatformHandler();

  get dialog(): HTMLDivElement {
    return assertInstanceof(
      this.shadowRoot?.getElementById('dialog'),
      HTMLDivElement,
    );
  }

  override updated(changedProperties: PropertyValues<this>): void {
    if (changedProperties.has('open')) {
      if (this.open) {
        this.dialog.showPopover();
      } else {
        this.dialog.hidePopover();
      }
    }
  }

  private close() {
    this.dispatchEvent(new Event('close'));
  }

  private renderDialog(
    imageName: string,
    header: string,
    description: RenderResult,
    buttons: RenderResult,
  ): RenderResult {
    // TODO(pihsun): Extract this to a separate component if any other place
    // need unclosable modal dialog.
    // We can't use <dialog> / <md-dialog> / <cra-dialog> here since the dialog
    // is always cancelable by pressing ESC, and the onboarding flow should not
    // be cancelable.
    // See https://issues.chromium.org/issues/346597066.
    return html`<div id="dialog" popover="manual">
      <div id="illust">
        <cra-image .name=${imageName}></cra-image>
      </div>
      <div id="content">
        <div id="header">${header}</div>
        <div id="description">${description}</div>
        <div id="buttons">${buttons}</div>
      </div>
    </div>`;
  }

  override render(): RenderResult {
    // Note that all the onboarding_ images are currently placeholders and
    // don't use dynamic color tokens yet.
    // TODO: b/344785475 - Change to final illustration when ready.
    switch (this.step.value) {
      case 0: {
        const nextStep = () => {
          this.step.value = 1;
        };
        return this.renderDialog(
          'onboarding_welcome',
          i18n.onboardingDialogWelcomeHeader,
          i18n.onboardingDialogWelcomeDescription,
          html`<cra-button
            .label=${i18n.onboardingDialogWelcomeNextButton}
            @click=${nextStep}
          ></cra-button>`,
        );
      }
      case 1: {
        const enableTranscription = () => {
          settings.mutate((s) => {
            s.transcriptionEnabled = TranscriptionEnableState.ENABLED;
          });
          this.platformHandler.installSoda();
          this.step.value = 2;
        };
        const disableTranscription = () => {
          settings.mutate((s) => {
            s.transcriptionEnabled = TranscriptionEnableState.DISABLED_FIRST;
          });
          this.close();
        };
        return this.renderDialog(
          'onboarding_transcription',
          i18n.onboardingDialogTranscriptionHeader,
          i18n.onboardingDialogTranscriptionDescription,
          html`
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionDeferButton}
              class="left"
              button-style="secondary"
              @click=${this.close}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionCancelButton}
              button-style="secondary"
              @click=${disableTranscription}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogTranscriptionTurnOnButton}
              @click=${enableTranscription}
            ></cra-button>
          `,
        );
      }
      case 2: {
        const disableSpeakerLabel = () => {
          settings.mutate((s) => {
            s.speakerLabelEnabled = SpeakerLabelEnableState.DISABLED_FIRST;
          });
          this.close();
        };

        const enableSpeakerLabel = () => {
          settings.mutate((s) => {
            s.speakerLabelEnabled = SpeakerLabelEnableState.ENABLED;
          });
          this.close();
        };

        return this.renderDialog(
          'onboarding_speaker_label',
          i18n.onboardingDialogSpeakerLabelHeader,
          html`<speaker-label-consent-dialog-content>
          </speaker-label-consent-dialog-content>`,
          html`
            <cra-button
              .label=${i18n.onboardingDialogSpeakerLabelDeferButton}
              class="left"
              button-style="secondary"
              @click=${this.close}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogSpeakerLabelDisallowButton}
              button-style="secondary"
              @click=${disableSpeakerLabel}
            ></cra-button>
            <cra-button
              .label=${i18n.onboardingDialogSpeakerLabelAllowButton}
              @click=${enableSpeakerLabel}
            ></cra-button>
          `,
        );
      }
      default:
        assertExhaustive(this.step.value);
    }
  }
}

window.customElements.define('onboarding-dialog', OnboardingDialog);

declare global {
  interface HTMLElementTagNameMap {
    'onboarding-dialog': OnboardingDialog;
  }
}
