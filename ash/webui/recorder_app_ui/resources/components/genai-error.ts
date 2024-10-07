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
  MAX_WORD_LENGTH,
  MIN_WORD_LENGTH,
} from '../core/on_device_model/ai_feature_constants.js';
import {
  GenaiResultType,
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
      font: var(--cros-button-1-font);
      gap: 16px;
      padding: 32px;
      text-align: center;
    }
  `;

  static override properties: PropertyDeclarations = {
    error: {attribute: false},
  };

  error: ModelResponseError|null = null;

  resultType: GenaiResultType|null = null;

  override render(): RenderResult {
    if (this.error === null) {
      return nothing;
    }

    let imageName: string;
    let message: string;
    switch (this.error) {
      case ModelResponseError.GENERAL:
        imageName = 'genai_error_general';
        message = i18n.genAiErrorGeneralLabel;
        break;

      case ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT: {
        imageName = 'genai_error_general';
        const resultType = assertExists(this.resultType);
        switch (resultType) {
          case GenaiResultType.SUMMARY:
            message =
              i18n.genAiErrorSummaryTranscriptTooShortLabel(MIN_WORD_LENGTH);
            break;
          case GenaiResultType.TITLE_SUGGESTION:
            message = i18n.genAiErrorTitleSuggestionTranscriptTooShortLabel(
              MIN_WORD_LENGTH,
            );
            break;
          default:
            assertExhaustive(resultType);
        }
        break;
      }

      case ModelResponseError.UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG: {
        imageName = 'genai_error_general';
        const resultType = assertExists(this.resultType);
        switch (resultType) {
          case GenaiResultType.SUMMARY:
            message =
              i18n.genAiErrorSummaryTranscriptTooLongLabel(MAX_WORD_LENGTH);
            break;
          case GenaiResultType.TITLE_SUGGESTION:
            message = i18n.genAiErrorTitleSuggestionTranscriptTooLongLabel(
              MAX_WORD_LENGTH,
            );
            break;
          default:
            assertExhaustive(resultType);
        }
        break;
      }

      case ModelResponseError.UNSAFE: {
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

    // TODO(pihsun): Add a "try again" button.
    return html`
      <cra-image .name=${imageName}></cra-image>
      <span>${message}</span>
    `;
  }
}

window.customElements.define('genai-error', GenaiError);

declare global {
  interface HTMLElementTagNameMap {
    'genai-error': GenaiError;
  }
}
