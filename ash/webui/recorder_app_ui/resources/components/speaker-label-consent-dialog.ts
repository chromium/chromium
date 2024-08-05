// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-feature-tour-dialog.js';
import './speaker-label-consent-dialog-content.js';

import {createRef, css, html, ref} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {settings, SpeakerLabelEnableState} from '../core/state/settings.js';

import {CraFeatureTourDialog} from './cra/cra-feature-tour-dialog.js';

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
    this.hide();
  }

  private enableSpeakerLabel() {
    settings.mutate((s) => {
      s.speakerLabelEnabled = SpeakerLabelEnableState.ENABLED;
    });
    this.hide();
  }

  override render(): RenderResult {
    // TODO: b/336963138 - Add correct link
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
          button-style="secondary"
          @click=${this.hide}
        ></cra-button>
        <cra-button
          .label=${i18n.onboardingDialogSpeakerLabelDisallowButton}
          button-style="secondary"
          @click=${this.disableSpeakerLabel}
        ></cra-button>
        <cra-button
          .label=${i18n.onboardingDialogSpeakerLabelAllowButton}
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
