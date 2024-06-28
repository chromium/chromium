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

import {DataDir} from './data_dir.js';
import {initContext} from './lit/context.js';
import {PlatformHandler} from './platform_handler.js';
import {RecordingDataManager} from './recording_data_manager.js';
import {installRouter} from './state/route.js';
import {init as initSettings, settingsSchema} from './state/settings.js';
import * as localStorage from './utils/local_storage.js';
import {ValidationError} from './utils/schema.js';

/**
 * Component for showing error message.
 *
 * For dogfooding before the stored data structure is stable, we check for
 * ValidationError and ask user to clear the storage and try again.
 *
 * TODO(pihsun): ValidationError should become an hard error after we have
 * stable metadata structure before launch.
 * TODO(pihsun): Integrate with metrics / error reporting.
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
      max-width: 960px;
      pointer-events: auto;
    }
  `;

  static override properties: PropertyDeclarations = {
    dataDir: {attribute: false},
    error: {attribute: false},
  };

  dataDir: DataDir|null = null;

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
    // TODO(pihsun): This is for dogfooding only, should remove this before
    // launch.
    if (this.error instanceof ValidationError) {
      if (this.error.issue.schema === settingsSchema) {
        // This is caused by settings schema change, clear the localStorage and
        // refresh.
        console.error(
          'Detected settings schema change, clear settings and reloading...',
        );
        localStorage.remove(localStorage.Key.SETTINGS);
        window.location.reload();
        return nothing;
      }
      if (this.dataDir !== null) {
        const clearRecordings = async () => {
          if (confirm('Really clear all recorder app data?')) {
            await this.dataDir?.clear();
            window.location.reload();
          }
        };
        return html`<div>
          Failed to validate stored data. This is likely caused by data format
          change. Try
          <a href="#" @click=${clearRecordings}>clear recordings.</a>. Please
          report this if the issue persists after clear recordings. (Note that
          this is only for early dogfooding when we still make
          backward-incompatible changes.)
          <pre>${errorDisplay}</pre>
        </div>`;
      }
      // Unknown validation error happened before dataDir is available,
      // fallthrough to generic case.
    }
    return html`<div>
      Unexpected error happened, please report this.
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

/**
 * Initializes all the systems that need to be ready before the app is mounted.
 */
export async function init(platformHandler: PlatformHandler): Promise<void> {
  let dataDir: DataDir|null = null;

  const errorView = new ErrorView();
  document.body.appendChild(errorView);
  function handleError(e: unknown) {
    console.error(e);
    errorView.dataDir = dataDir;
    errorView.error = e;
  }

  window.addEventListener('unhandledrejection', (rejection) => {
    handleError(rejection.reason);
  });
  window.addEventListener('error', (errorEvent) => {
    handleError(errorEvent.error);
  });

  installRouter();
  initSettings();
  dataDir = await DataDir.createFromOpfs();
  const recordingDataManager = await RecordingDataManager.create(dataDir);
  initContext({
    recordingDataManager,
    platformHandler,
  });
}
