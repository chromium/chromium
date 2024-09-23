// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './dev-page.js';
import './main-page.js';
import './playback-page.js';
import './record-page.js';

import {
  createRef,
  css,
  html,
  nothing,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {usePlatformHandler} from '../core/lit/context.js';
import {ModelState} from '../core/on_device_model/types.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {currentRoute} from '../core/state/route.js';
import {settings} from '../core/state/settings.js';
import {assertExists} from '../core/utils/assert.js';

import {MainPage} from './main-page.js';
import {PlaybackPage} from './playback-page.js';
import {RecordPage} from './record-page.js';

function toBoolean(s: string|null): boolean {
  return s === 'true';
}

/**
 * Root route of Recorder App.
 */
export class RecorderApp extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
      height: 100%;
      width: 100%;
    }
  `;

  private readonly mainPageRef = createRef<MainPage>();

  private readonly playbackPage = createRef<PlaybackPage>();

  private readonly recordPage = createRef<RecordPage>();

  private readonly platformHandler = usePlatformHandler();

  get mainPage(): MainPage {
    return assertExists(this.mainPageRef.value);
  }

  get mainPageForTest(): MainPage {
    return assertExists(this.mainPageRef.value);
  }

  get playbackPageForTest(): PlaybackPage {
    return assertExists(this.playbackPage.value);
  }

  get recordPageForTest(): RecordPage {
    return assertExists(this.recordPage.value);
  }

  private render404() {
    return 'Not found';
  }

  override firstUpdated(): void {
    const summaryState = this.platformHandler.summaryModelLoader.state;
    const titleState = this.platformHandler.titleSuggestionModelLoader.state;
    const sodaState = this.platformHandler.sodaState;

    function isAvailable(state: ModelState) {
      return state.kind !== 'unavailable' && state.kind !== 'error';
    }

    this.platformHandler.eventsSender.sendStartSessionEvent({
      speakerLabelEnableState: settings.value.speakerLabelEnabled,
      summaryAvailable: isAvailable(summaryState.value),
      summaryEnableState: settings.value.summaryEnabled,
      titleSuggestionAvailable: isAvailable(titleState.value),
      transcriptionAvailable: isAvailable(sodaState.value),
      transcriptionEnableState: settings.value.transcriptionEnabled,
    });
    this.platformHandler.perfLogger.finish('appStart');
  }

  override render(): RenderResult {
    if (currentRoute.value === null) {
      return nothing;
    }

    const route = currentRoute.value;

    switch (route.name) {
      case 'main':
        return html`<main-page
          ${ref(this.mainPageRef)}
          exportparts="actions:main-page-actions"
        >
        </main-page>`;
      case 'playback':
        return html`<playback-page
          .recordingId=${route.parameters.id}
          ${ref(this.playbackPage)}
        >
        </playback-page>`;
      case 'record': {
        const {includeSystemAudio, micId} = route.parameters;
        return html`<record-page
          .includeSystemAudio=${toBoolean(includeSystemAudio)}
          .micId=${micId}
          part="record-page"
          exportparts="container:record-page-container"
          ${ref(this.recordPage)}
        >
        </record-page>`;
      }
      case 'dev':
        return html`<dev-page></dev-page>`;
      case 'test':
        return nothing;
      default:
        return this.render404();
    }
  }
}

window.customElements.define('recorder-app', RecorderApp);

declare global {
  interface HTMLElementTagNameMap {
    'recorder-app': RecorderApp;
  }
}
