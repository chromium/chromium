// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  css,
  html,
  LitElement,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import * as localStorage from '../../core/utils/local_storage.js';
import {ValidationError} from '../../core/utils/schema.js';

import {devSettingsSchema} from './settings.js';

/**
 * Component for showing error message for dev server.
 */
export class ErrorView extends LitElement {
  static override styles = css`
    :host {
      align-items: center;
      display: flex;
      font-family: monospace;
      inset: 0;
      justify-content: center;
      pointer-events: none;
      position: absolute;
      z-index: 100;
    }

    div {
      background: var(--cros-sys-surface);
      border: 1px solid var(--cros-sys-outline);
      max-width: 960px;
      padding: 8px;
      pointer-events: auto;
    }
  `;

  static override properties: PropertyDeclarations = {
    error: {attribute: false},
  };

  error: unknown = null;

  override render(): RenderResult {
    if (this.error === null) {
      return nothing;
    }
    const errorDisplay = (() => {
      if (this.error instanceof Error) {
        return this.error.stack;
      } else {
        return this.error;
      }
    })();
    if (this.error instanceof ValidationError &&
        this.error.issue.schema === devSettingsSchema) {
      // This is caused by dev settings schema change, clear the localStorage
      // and refresh.
      console.error('Detected dev settings schema change...');
      localStorage.remove(localStorage.Key.DEV_SETTINGS);
      window.location.reload();
      return nothing;
    }
    return html`<div>
      Unexpected error happened.
      <pre>${errorDisplay}</pre>
    </div>`;
  }
}

window.customElements.define('error-view', ErrorView);

declare global {
  interface HTMLElementTagNameMap {
    'error-view': ErrorView;
  }
}
