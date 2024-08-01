// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-feature-tour-dialog.js';

import {createRef, css, html, ref} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {settings, SpeakerIdEnableState} from '../core/state/settings.js';

import {CraFeatureTourDialog} from './cra/cra-feature-tour-dialog.js';

/**
 * Dialog for asking speaker ID consent from user.
 *
 * Note that this is different from onboarding dialog and is only used when
 * user defers speaker ID consent on onboarding, and then enable it later.
 *
 * The main difference to the onboarding dialog is that onboarding dialog is
 * not dismissable from user by clicking outside / pressing ESC, but this
 * dialog is, so this dialog can use cra-dialog as underlying implementation and
 * the onboarding dialog needs to be implemented manually, makes it hard for
 * these two to share implementation.
 *
 * TODO(pihsun): Consider other way to share part of the implementation.
 */
export class SpeakerIdConsentDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    .left {
      margin-right: auto;
    }
  `;

  private readonly dialog = createRef<CraFeatureTourDialog>();

  async show(): Promise<void> {
    await this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.hide();
  }

  private disableSpeakerId() {
    settings.mutate((s) => {
      s.speakerIdEnabled = SpeakerIdEnableState.DISABLED_FIRST;
    });
    this.hide();
  }

  private enableSpeakerId() {
    settings.mutate((s) => {
      s.speakerIdEnabled = SpeakerIdEnableState.ENABLED;
    });
    this.hide();
  }

  override render(): RenderResult {
    // TODO: b/336963138 - Add correct link
    return html`<cra-feature-tour-dialog
      ${ref(this.dialog)}
      illustrationName="onboarding_speaker_id"
      header=${i18n.onboardingDialogSpeakerIdHeader}
    >
      <div slot="content">
        ${i18n.onboardingDialogSpeakerIdDescription}
        <a href="javascript:;"
          >${i18n.onboardingDialogSpeakerIdLearnMoreLink}</a
        >
      </div>
      <div slot="actions">
        <cra-button
          .label=${i18n.onboardingDialogSpeakerIdDeferButton}
          class="left"
          button-style="secondary"
          @click=${this.hide}
        ></cra-button>
        <cra-button
          .label=${i18n.onboardingDialogSpeakerIdDisallowButton}
          button-style="secondary"
          @click=${this.disableSpeakerId}
        ></cra-button>
        <cra-button
          .label=${i18n.onboardingDialogSpeakerIdAllowButton}
          @click=${this.enableSpeakerId}
        ></cra-button>
      </div>
    </cra-feature-tour-dialog>`;
  }
}

window.customElements.define(
  'speaker-id-consent-dialog',
  SpeakerIdConsentDialog,
);

declare global {
  interface HTMLElementTagNameMap {
    'speaker-id-consent-dialog': SpeakerIdConsentDialog;
  }
}
