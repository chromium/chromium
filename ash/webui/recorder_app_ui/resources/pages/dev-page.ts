// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mwc/@material/web/button/filled-button.js';
import 'chrome://resources/mwc/@material/web/icon/icon.js';
import 'chrome://resources/mwc/@material/web/iconbutton/icon-button.js';
import '../components/cra/cra-icon.js';

import {
  createRef,
  css,
  html,
  nothing,
  ref,
  when,
} from 'chrome://resources/mwc/lit/index.js';

import {
  usePlatformHandler,
  useRecordingDataManager,
} from '../core/lit/context.js';
import {Model, ModelResponse} from '../core/on_device_model/types.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';

/**
 * Dev page of Recorder App.
 */
export class DevPage extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
      height: 100%;
      position: relative;
      width: 100%;
    }

    .container {
      display: flex;
      flex-direction: column;
      gap: 8px;
      height: 100%;
      width: 100%;
    }

    .section {
      align-items: center;
      background: var(--cros-sys-surface1);
      display: flex;
      flex-direction: column;
      padding: 8px;
    }

    md-icon-button {
      align-self: start;
    }
  `;

  private readonly platformHandler = usePlatformHandler();

  readonly recordingDataManager = useRecordingDataManager();

  // TODO(pihsun): Better loading state handling. The following two should
  // probably come together from some Promise -> signal utility?
  private readonly modelLoading = signal(false);

  private readonly titleSuggestionModel = signal<Model<string[]>|null>(null);

  private readonly textareaRef = createRef<HTMLTextAreaElement>();

  /**
   * Contains model response of the suggested titles.
   */
  private readonly titles = signal<ModelResponse<string[]>|null>(null);

  override disconnectedCallback(): void {
    if (this.titleSuggestionModel.value !== null) {
      this.titleSuggestionModel.value.close();
      this.titleSuggestionModel.value = null;
    }
  }

  private async onSuggestTitleClick() {
    if (this.titleSuggestionModel.value === null) {
      return;
    }
    const value = this.textareaRef.value?.value;
    if (value === undefined) {
      return;
    }
    // TODO(shik): Add loading state.
    this.titles.value = await this.titleSuggestionModel.value.execute(value);
  }

  private async onClearClicked() {
    await this.recordingDataManager.clear();
  }

  private async onLoadModelClicked() {
    this.modelLoading.value = true;
    try {
      this.titleSuggestionModel.value =
        await this.platformHandler.titleSuggestionModelLoader.load();
    } finally {
      // TODO(pihsun): Display / handle error better.
      this.modelLoading.value = false;
    }
  }

  private renderLoadModelResult() {
    if (this.modelLoading.value) {
      return 'Loading...';
    }
    if (this.titleSuggestionModel.value !== null) {
      return 'Model loaded';
    }
    return nothing;
  }

  override render(): RenderResult {
    // TODO(shik): Make this prettier.
    return html`
      <div class="container">
        <md-icon-button href="/">
          <cra-icon name="arrow_back"></cra-icon>
        </md-icon-button>
        <div class="section">
          <md-filled-button @click=${this.onClearClicked}
            >Clear OPFS</md-filled-button
          >
        </div>
        <div class="section">
          <md-filled-button @click=${this.onLoadModelClicked}
            >Load Model</md-filled-button
          >
          <span>${this.renderLoadModelResult()}</span>
        </div>
        ${
      when(
        this.titleSuggestionModel.value !== null,
        () => html`
            <div class="section">
              <textarea ${ref(this.textareaRef)} rows=${5}></textarea>
              <pre>Titles: ${JSON.stringify(this.titles.value, null, 2)}</pre>
              <md-filled-button @click=${this.onSuggestTitleClick}>
                Suggest Title
              </md-filled-button>
            </div>
          `,
      )}
        ${this.platformHandler.renderDevUi()}
      </div>
    `;
  }
}

window.customElements.define('dev-page', DevPage);

declare global {
  interface HTMLElementTagNameMap {
    'dev-page': DevPage;
  }
}
