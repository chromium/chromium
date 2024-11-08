// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './cra/cra-icon.js';
import './settings-row.js';

import {
  css,
  html,
  map,
  nothing,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {LangPackInfo, LanguageCode} from '../core/soda/language_info.js';
import {settings} from '../core/state/settings.js';
import {setTranscriptionLanguage} from '../core/state/transcription.js';
import {assertExhaustive} from '../core/utils/assert.js';

import {withTooltip} from './directives/with-tooltip.js';

export class LanguagePicker extends ReactiveLitElement {
  static override styles = css`
    :host {
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

    #header {
      align-items: center;
      color: var(--cros-sys-primary);
      display: flex;
      flex-flow: row;
      gap: 16px;
      padding: 16px 8px;
      position: relative;

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

    // TODO: b/377885042 - Move the circular progress to a separate component.
    settings-row cra-button md-circular-progress {
      --md-circular-progress-active-indicator-color: var(--cros-sys-disabled);

      /*
       * This has a lower precedence than the size override in cros-button,
       * but still need to be set to have correct line width.
       */
      --md-circular-progress-size: 24px;

      /*
       * This is to override the size setting for slotted element in
       * cros-button. On figma the circular progress have 2px padding, but
       * md-circular-progres has a non-configurable 4px padding. Setting a
       * negative margin so the extra padding doesn't expand the button size.
       */
      height: 24px;
      margin: -2px;
      width: 24px;
    }
  `;

  private readonly platformHandler = usePlatformHandler();

  private onCloseClick() {
    this.dispatchEvent(new Event('close'));
  }

  private renderLanguageRow(
    language: LangPackInfo,
    selectedLanguage: LanguageCode|null,
  ): RenderResult {
    const sodaState =
      this.platformHandler.getSodaState(language.languageCode).value;
    if (sodaState.kind === 'unavailable') {
      return nothing;
    }

    const name = html`
      <span slot="label">${language.displayName}</span>
    `;

    function onSelectAndDownload() {
      setTranscriptionLanguage(language.languageCode);
    }

    const downloadButton = html`
      <cra-button
        slot="action"
        button-style="secondary"
        .label=${i18n.languagePickerLanguageDownloadButton}
        @click=${onSelectAndDownload}
      ></cra-button>
    `;
    switch (sodaState.kind) {
      case 'notInstalled': {
        return html`
        <settings-row>
          ${name}
          ${downloadButton}
        </settings-row>
        `;
      }
      // Shows the download button for users to try again.
      case 'error': {
        return html`
        <settings-row>
          ${name}
          <span slot="description" class="error">
            ${i18n.languagePickerLanguageErrorDescription}
          </span>
          ${downloadButton}
        </settings-row>
        `;
      }
      case 'installing': {
        const progressDescription =
          i18n.languagePickerLanguageDownloadingProgressDescription(
            sodaState.progress,
          );
        return html`
          <settings-row>
            ${name}
            <span slot="description">${progressDescription}</span>
            <cra-button
              slot="action"
              button-style="secondary"
              .label=${i18n.languagePickerLanguageDownloadingButton}
              disabled
            >
              <md-circular-progress indeterminate slot="leading-icon">
              </md-circular-progress>
            </cra-button>
          </settings-row>
        `;
      }
      case 'installed': {
        if (language.languageCode === selectedLanguage) {
          return html`
          <settings-row>
            ${name}
            <cra-icon slot="action" name="checked"></cra-icon>
          </settings-row>
          `;
        } else {
          // Set and install the language to avoid inconsistent SODA state.
          // TODO: b/375306309 - Separate set and install steps when the state
          // become consistent after implementing `OnSodaUninstalled`.
          return html`
          <settings-row>
            ${name}
            <span slot="action"
              @click=${onSelectAndDownload}
            >
            </span>
          </settings-row>
          `;
        }
      }
      default:
        return assertExhaustive(sodaState.kind);
    }
  }

  private renderSelectedLanguage(): RenderResult {
    const selectedLanguage = settings.value.transcriptionLanguage;
    if (selectedLanguage === null) {
      return html`
        <settings-row>
          <span slot="label">
            ${i18n.languagePickerSelectedLanguageNoneLabel}
          </span>
        </settings-row>
      `;
    }
    const sodaState = this.platformHandler.getSodaState(selectedLanguage).value;
    if (sodaState.kind !== 'installed' && sodaState.kind !== 'installing') {
      return html`
        <settings-row>
          <span slot="label">
            ${i18n.languagePickerSelectedLanguageNoneLabel}
          </span>
        </settings-row>
      `;
    }
    return this.renderLanguageRow(
      this.platformHandler.getLangPackInfo(selectedLanguage),
      selectedLanguage,
    );
  }

  private renderAvailableLanguages(): RenderResult {
    const list = this.platformHandler.getLangPackList();
    const selectedLanguage = settings.value.transcriptionLanguage;
    return map(
      list,
      (langPack) => this.renderLanguageRow(langPack, selectedLanguage),
    );
  }

  override render(): RenderResult {
    // TODO: b/377885042 - Render "close" button when language picker is not
    // inside the setting menu.
    return html`
      <div id="header">
        <cra-icon-button
          id="back"
          buttonstyle="floating"
          size="small"
          shape="circle"
          aria-label=${i18n.languagePickerBackButtonAriaLabel}
          ${withTooltip(i18n.languagePickerBackButtonTooltip)}
          @click=${this.onCloseClick}
        >
          <cra-icon slot="icon" name="arrow_back"></cra-icon>
        </cra-icon-button>
        <h3>${i18n.languagePickerHeader}</h3>
      </div>
      <div id="content">
        <div class="section">
          <h4 class="title">${i18n.languagePickerSelectedLanguageHeader}</h4>
          <div class="body">
          ${this.renderSelectedLanguage()}
          </div>
        </div>
        <div class="section">
          <h4 class="title">${i18n.languagePickerAvailableLanguagesHeader}</h4>
          <div class="body">
          ${this.renderAvailableLanguages()}
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
