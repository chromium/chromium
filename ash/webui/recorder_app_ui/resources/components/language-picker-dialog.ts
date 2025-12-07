// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './settings-row.js';
import './spoken-message.js';
import './language-list.js';

import {createRef, css, html, ref} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {Signal, signal} from '../core/reactive/signal.js';
import {LanguageCode} from '../core/soda/language_info.js';
import {
  installSoda,
  setTranscriptionLanguage,
} from '../core/state/transcription.js';

import {CraDialog} from './cra/cra-dialog.js';
import {withTooltip} from './directives/with-tooltip.js';

/**
 * Stand-alone language selection for users to choose and download transcript
 * language during recording.
 */
export class LanguagePickerDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: contents;
    }

    cra-dialog {
      --md-dialog-container-color: var(--cros-sys-surface3);

      /* 16px margin for each side at minimum size. */
      max-height: calc(100% - 32px);
      max-width: calc(100% - 32px);
      width: 512px;

      @container style(--dark-theme: 1) {
        /*
         * TODO: b/336963138 - This is neutral5 in spec but there's no
         * neutral5 in colors.css.
         */
        --md-dialog-container-color: var(--cros-sys-app_base_shaded);
      }
    }

    div[slot="content"] {
      padding: 0;
    }

    #header {
      padding: 24px;
      position: relative;

      & > h2 {
        color: var(--cros-sys-primary);
        font: var(--cros-title-1-font);
        margin: unset;
      }

      & > cra-icon-button {
        position: absolute;
        right: 16px;
        top: 16px;
      }
    }

    #body {
      background: var(--cros-sys-surface1);
      border-radius: 20px;
      display: flex;
      flex-flow: column;
      gap: 8px;
      padding: 0 16px 16px;

      @container style(--dark-theme: 1) {
        background: var(--cros-sys-app_base);
      }
    }

    .section {
      padding-top: 8px;

      & > .title {
        color: var(--cros-sys-primary);
        font: var(--cros-button-2-font);
        margin: unset;
        padding: 8px;
      }

      & > .body {
        border-radius: 16px;
        display: flex;
        flex-flow: column;
        gap: 1px;

        /* To have the border-radius applied to content. */
        overflow: hidden;
      }
    }
  `;

  private readonly platformHandler = usePlatformHandler();

  private readonly selectedLanguage: Signal<LanguageCode|null> = signal(null);

  private readonly dialog = createRef<CraDialog>();

  async show(): Promise<void> {
    await this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.close();
  }

  private onDialogClose() {
    // Only sets language after users close the dialog.
    if (this.selectedLanguage.value === null) {
      return;
    }
    setTranscriptionLanguage(this.selectedLanguage.value);
  }

  private onLanguageSelect(ev: CustomEvent<LanguageCode>) {
    this.selectedLanguage.value = ev.detail;
  }

  private onLanguageDownloadClick(ev: CustomEvent<LanguageCode>) {
    installSoda(ev.detail);
    this.selectedLanguage.value = ev.detail;
  }

  private renderSelectedLanguage(): RenderResult {
    const noSelectionRow = html`
      <settings-row>
        <span slot="label">
          ${i18n.languagePickerSelectedLanguageNoneLabel}
        </span>
      </settings-row>
    `;
    if (this.selectedLanguage.value === null) {
      return noSelectionRow;
    }
    const selectedLanguage = this.selectedLanguage.value;
    const sodaState = this.platformHandler.getSodaState(selectedLanguage).value;
    if (sodaState.kind !== 'installed' && sodaState.kind !== 'installing') {
      return noSelectionRow;
    }
    const name =
      this.platformHandler.getLangPackInfo(selectedLanguage).displayName;
    const status = sodaState.kind === 'installing' ?
      i18n.languagePickerLanguageDownloadingAriaLabel(
        name,
        sodaState.progress,
      ) :
      i18n.languagePickerLanguageSelectedAriaLabel(name);
    return html`
      <settings-row>
        <span slot="label" aria-hidden="true">${name}</span>
        <spoken-message slot="status">${status}</spoken-message>
      </settings-row>
    `;
  }

  override render(): RenderResult {
    return html`
      <cra-dialog
        id="language-selector"
        @close=${this.onDialogClose}
        ${ref(this.dialog)}
      >
        <div id="header" slot="headline">
          <h2>${i18n.languagePickerHeader}</h2>
          <cra-icon-button
            buttonstyle="floating"
            size="small"
            shape="circle"
            @click=${this.hide}
            aria-label=${i18n.closeDialogButtonTooltip}
            ${withTooltip()}
          >
            <cra-icon slot="icon" name="close"></cra-icon>
          </cra-icon-button>
        </div>
        <div id="body" slot="content">
          <div class="section">
            <h3 class="title">${i18n.languagePickerSelectedLanguageHeader}</h3>
            <div class="body">${this.renderSelectedLanguage()}</div>
          </div>
          <div class="section">
            <h3 class="title">
              ${i18n.languagePickerAvailableLanguagesHeader}
            </h3>
            <language-list
              class="body"
              role="region"
              aria-label=${i18n.languagePickerLanguagesListLandmarkAriaLabel}
              .selectedLanguage=${this.selectedLanguage.value}
              @language-select-click=${this.onLanguageSelect}
              @language-download-click=${this.onLanguageDownloadClick}
            >
            </language-list>
          </div>
        </div>
      </cra-dialog>`;
  }
}

window.customElements.define('language-picker-dialog', LanguagePickerDialog);

declare global {
  interface HTMLElementTagNameMap {
    'language-picker-dialog': LanguagePickerDialog;
  }
}
