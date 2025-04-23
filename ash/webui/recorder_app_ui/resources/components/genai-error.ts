// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-image.js';

import {
  css,
  CSSResultGroup,
  html,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {
  GenaiResultType,
  ModelExecutionError,
  ModelLoadError,
  ModelResponseError,
} from '../core/on_device_model/types.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {assertExhaustive, assertExists} from '../core/utils/assert.js';

export class GenaiError extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    :host {
      align-items: center;
      display: flex;
      flex-flow: column;
      gap: 16px;
      padding: 32px;
    }

    #description {
      align-items: center;
      display: flex;
      flex-flow: column;
      font: var(--cros-button-1-font);
      gap: 8px;
      text-align: center;

      & > button {
        font: var(--cros-button-2-font);
        background: none;
        border: none;
        color: var(--cros-sys-primary);
        cursor: pointer;
        margin: 0;
        padding: 0;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    error: {attribute: false},
  };

  error: ModelResponseError|null = null;

  resultType: GenaiResultType|null = null;

  private onDownloadClick() {
    this.dispatchEvent(new CustomEvent('download-clicked'));
  }

  override render(): RenderResult {
    if (this.error === null) {
      return nothing;
    }

    let imageName: string;
    let message: string;
    let action: RenderResult = nothing;
    switch (this.error) {
      case ModelLoadError.LOAD_FAILURE:
        imageName = 'genai_error_general';
        message = i18n.genAiErrorModelLoadFailureLabel;
        // Use native button element to make text clickable.
        action = html`
          <button
            aria-label=${i18n.genAiErrorModelDownloadButtonAriaLabel}
            @click=${this.onDownloadClick}
          >${i18n.genAiErrorModelDownloadButton}</button>
        `;
        break;
      case ModelLoadError.NEEDS_REBOOT: {
        imageName = 'genai_error_general';
        const resultType = assertExists(this.resultType);
        switch (resultType) {
          case GenaiResultType.SUMMARY:
            message = i18n.genAiErrorSummaryNeedsRebootLabel;
            break;
          case GenaiResultType.TITLE_SUGGESTION:
            message = i18n.genAiErrorTitleSuggestionNeedsRebootLabel;
            break;
          default:
            assertExhaustive(resultType);
        }
        break;
      }
      case ModelExecutionError.GENERAL:
        imageName = 'genai_error_general';
        message = i18n.genAiErrorGeneralLabel;
        break;
      case ModelExecutionError.UNSUPPORTED_LANGUAGE: {
        imageName = 'genai_error_unsafe';
        const resultType = assertExists(this.resultType);
        switch (resultType) {
          case GenaiResultType.SUMMARY:
            message = i18n.genAiErrorSummaryLanguageUnsupportedLabel;
            break;
          case GenaiResultType.TITLE_SUGGESTION:
            message = i18n.genAiErrorTitleSuggestionLanguageUnsupportedLabel;
            break;
          default:
            assertExhaustive(resultType);
        }
        break;
      }
      case ModelExecutionError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT: {
        imageName = 'genai_error_general';
        const resultType = assertExists(this.resultType);
        switch (resultType) {
          case GenaiResultType.SUMMARY:
            message = i18n.genAiErrorSummaryTranscriptTooShortLabel;
            break;
          case GenaiResultType.TITLE_SUGGESTION:
            message = i18n.genAiErrorTitleSuggestionTranscriptTooShortLabel;
            break;
          default:
            assertExhaustive(resultType);
        }
        break;
      }

      case ModelExecutionError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG: {
        imageName = 'genai_error_general';
        const resultType = assertExists(this.resultType);
        switch (resultType) {
          case GenaiResultType.SUMMARY:
            message = i18n.genAiErrorSummaryTranscriptTooLongLabel;
            break;
          case GenaiResultType.TITLE_SUGGESTION:
            message = i18n.genAiErrorTitleSuggestionTranscriptTooLongLabel;
            break;
          default:
            assertExhaustive(resultType);
        }
        break;
      }

      case ModelExecutionError.UNSAFE: {
        imageName = 'genai_error_unsafe';
        const resultType = assertExists(this.resultType);
        switch (resultType) {
          case GenaiResultType.SUMMARY:
            message = i18n.genAiErrorSummaryTrustAndSafetyLabel;
            break;
          case GenaiResultType.TITLE_SUGGESTION:
            message = i18n.genAiErrorTitleSuggestionTrustAndSafetyLabel;
            break;
          default:
            assertExhaustive(resultType);
        }
        break;
      }
      default:
        assertExhaustive(this.error);
    }

    return html`
      <cra-image .name=${imageName}></cra-image>
      <div id="description">
        <span>${message}</span>
        ${action}
      </div>
    `;
  }
}

window.customElements.define('genai-error', GenaiError);

declare global {
  interface HTMLElementTagNameMap {
    'genai-error': GenaiError;
  }
}
