// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './settings-row.js';
import './spoken-message.js';
import './language-list.js';

import {
  createRef,
  css,
  html,
  PropertyValues,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {LanguageCode} from '../core/soda/language_info.js';
import {setTranscriptionLanguage} from '../core/state/transcription.js';
import {assertExists} from '../core/utils/assert.js';

import {CraButton} from './cra/cra-button.js';
import {withTooltip} from './directives/with-tooltip.js';

/**
 * Language selection for users to choose and download transcript language.
 */
export class LanguagePicker extends ReactiveLitElement {
  static override styles = css`
    :host {
      border-radius: 20px;
      display: block;
    }

    #root {
      display: flex;
      flex-flow: column;
      gap: 8px;
      padding: 0 16px 16px;
    }

    #header {
      align-items: center;
      color: var(--cros-sys-primary);
      display: flex;
      flex-flow: row;
      gap: 16px;
      padding: 16px 8px;

      & > h3 {
        font: var(--cros-button-1-font);
        margin: unset;
      }

      & > #back {
        --cros-icon-button-color-override: var(--cros-sys-primary);
        --cros-icon-button-icon-size: 20px;

        margin: 0;
      }
    }

    #content {
      display: flex;
      flex-flow: column;
      gap: 16px;
    }

    .section {
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

  private readonly backButton = createRef<CraButton>();

  private onCloseClick() {
    this.dispatchEvent(new Event('close'));
  }

  private onSelectAndDownload(ev: CustomEvent<LanguageCode>) {
    setTranscriptionLanguage(ev.detail);
  }

  private renderSelectedLanguage(
    selectedLanguage: LanguageCode|null,
  ): RenderResult {
    const noSelectionRow = html`
      <settings-row>
        <span slot="label">
          ${i18n.languagePickerSelectedLanguageNoneLabel}
        </span>
      </settings-row>
    `;
    if (selectedLanguage === null) {
      return noSelectionRow;
    }
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

  protected override firstUpdated(_changedProperties: PropertyValues): void {
    const backButton = assertExists(this.backButton.value);
    backButton.updateComplete.then(() => {
      backButton.focus();
    });
  }

  override render(): RenderResult {
    const selectedLanguage = this.platformHandler.getSelectedLanguage();
    return html`
      <div id="root">
        <div id="header">
          <cra-icon-button
            id="back"
            buttonstyle="floating"
            size="small"
            shape="circle"
            aria-label=${i18n.languagePickerBackButtonAriaLabel}
            @click=${this.onCloseClick}
            ${withTooltip(i18n.languagePickerBackButtonTooltip)}
            ${ref(this.backButton)}
          >
            <cra-icon slot="icon" name="arrow_back"></cra-icon>
          </cra-icon-button>
          <h3>${i18n.languagePickerHeader}</h3>
        </div>
        <div id="content">
          <div class="section">
            <h4 class="title">${i18n.languagePickerSelectedLanguageHeader}</h4>
            <div class="body">
              ${this.renderSelectedLanguage(selectedLanguage)}
            </div>
          </div>
          <div class="section">
            <h4 class="title">
              ${i18n.languagePickerAvailableLanguagesHeader}
            </h4>
            <language-list
              class="body"
              role="region"
              aria-label=${i18n.languagePickerLanguagesListLandmarkAriaLabel}
              .selectedLanguage=${selectedLanguage}
              @language-select-click=${this.onSelectAndDownload}
              @language-download-click=${this.onSelectAndDownload}
            >
            </language-list>
          </div>
        </div>
      </div>
    `;
  }
}

window.customElements.define('language-picker', LanguagePicker);

declare global {
  interface HTMLElementTagNameMap {
    'language-picker': LanguagePicker;
  }
}
