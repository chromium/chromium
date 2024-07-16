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
import {ModelResponseError} from '../core/platform_handler.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';

export class GenaiError extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    :host {
      align-items: center;
      display: flex;
      flex-flow: column;
      font: var(--cros-button-1-font);
      gap: 16px;
      padding: 32px;
    }
  `;

  static override properties: PropertyDeclarations = {
    error: {attribute: false},
  };

  error: ModelResponseError|null = null;

  // Note that the `i18n` use context so it can't be put in module scope.
  private readonly imageNameMessageMap = {
    [ModelResponseError.GENERAL]: {
      imageName: 'genai_error_general',
      message: i18n.genAiErrorGeneralLabel,
    },
    [ModelResponseError.UNSAFE]: {
      imageName: 'genai_error_unsafe',
      message: i18n.genAiErrorTrustAndSafetyLabel,
    },
  } as const;

  override render(): RenderResult {
    if (this.error === null) {
      return nothing;
    }

    const {imageName, message} = this.imageNameMessageMap[this.error];
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
