// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-feature-tour-dialog.js';
import './speaker-label-consent-dialog-content.js';

import {createRef, css, html, ref} from 'chrome://resources/mwc/lit/index.js';

import {i18n, NoArgStringName} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {settings, SpeakerLabelEnableState} from '../core/state/settings.js';

import {CraFeatureTourDialog} from './cra/cra-feature-tour-dialog.js';
import {
  DESCRIPTION_NAMES as SPEAKER_LABEL_DIALOG_DESCRIPTION_NAMES,
} from './speaker-label-consent-dialog-content.js';

const ALLOW_BUTTON_NAME: NoArgStringName =
  'onboardingDialogSpeakerLabelAllowButton';
const DISALLOW_BUTTON_NAME: NoArgStringName =
  'onboardingDialogSpeakerLabelDisallowButton';

/**
 * Dialog for asking speaker label consent from user.
 *
 * Note that this is different from onboarding dialog and is only used when
 * user defers speaker label consent on onboarding, and then enable it later.
 *
 * The main difference to the onboarding dialog is that onboarding dialog is
 * not dismissable from user by clicking outside / pressing ESC, but this
 * dialog is, so this dialog can use cra-dialog as underlying implementation and
 * the onboarding dialog needs to be implemented manually, makes it hard for
 * these two to share implementation.
 *
 * TODO(pihsun): Consider other way to share part of the implementation.
 */
export class SpeakerLabelConsentDialog extends ReactiveLitElement {
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

  private readonly platformHandler = usePlatformHandler();

  async show(): Promise<void> {
    await this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.hide();
  }

  private disableSpeakerLabel() {
    settings.mutate((s) => {
      s.speakerLabelEnabled = SpeakerLabelEnableState.DISABLED_FIRST;
    });
    this.platformHandler.recordSpeakerLabelConsent(
      false,
      SPEAKER_LABEL_DIALOG_DESCRIPTION_NAMES,
      DISALLOW_BUTTON_NAME,
    );
    this.hide();
  }

  private enableSpeakerLabel() {
    settings.mutate((s) => {
      s.speakerLabelEnabled = SpeakerLabelEnableState.ENABLED;
    });
    this.platformHandler.recordSpeakerLabelConsent(
      true,
      SPEAKER_LABEL_DIALOG_DESCRIPTION_NAMES,
      ALLOW_BUTTON_NAME,
    );
    this.hide();
  }

  override render(): RenderResult {
    return html`<cra-feature-tour-dialog
      ${ref(this.dialog)}
      illustrationName="onboarding_speaker_label"
      header=${i18n.onboardingDialogSpeakerLabelHeader}
    >
      <speaker-label-consent-dialog-content slot="content">
      </speaker-label-consent-dialog-content>
      <div slot="actions">
        <cra-button
          .label=${i18n.onboardingDialogSpeakerLabelDeferButton}
          class="left"
          @click=${this.hide}
        ></cra-button>
        <cra-button
          .label=${i18n[DISALLOW_BUTTON_NAME]}
          @click=${this.disableSpeakerLabel}
        ></cra-button>
        <cra-button
          .label=${i18n[ALLOW_BUTTON_NAME]}
          @click=${this.enableSpeakerLabel}
        ></cra-button>
      </div>
    </cra-feature-tour-dialog>`;
  }
}

window.customElements.define(
  'speaker-label-consent-dialog',
  SpeakerLabelConsentDialog,
);

declare global {
  interface HTMLElementTagNameMap {
    'speaker-label-consent-dialog': SpeakerLabelConsentDialog;
  }
}
