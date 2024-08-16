// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/badge/badge.js';
import 'chrome://resources/mwc/@material/web/progress/circular-progress.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './genai-error.js';
import './genai-feedback-buttons.js';
import './genai-placeholder.js';
import './summary-consent-card.js';

import {
  classMap,
  css,
  CSSResultGroup,
  html,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ModelResponse} from '../core/on_device_model/types.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {Transcription} from '../core/soda/soda.js';
import {settings, SummaryEnableState} from '../core/state/settings.js';
import {assertExhaustive} from '../core/utils/assert.js';

import {GenaiResultType} from './genai-error.js';

export class SummarizationView extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    :host {
      display: block;
    }

    /*
     * Set display: none when there's nothing to show, so this won't introduce
     * an additional "blank" box to the parent and result in e.g. one more row
     * in the flex layout.
     */
    :host(.empty) {
      display: none;
    }

    #container {
      border-radius: 12px;
      display: flex;
      flex-flow: column;
      gap: 4px;

      /* To have the border-radius applied to content. */
      overflow: hidden;
    }

    .sheet {
      background-color: var(--cros-sys-app_base_shaded);
      border-radius: 4px;
    }

    #header {
      align-items: center;
      display: flex;
      font: var(--cros-button-1-font);
      gap: 4px;
      height: 48px;
      padding: 0 12px;
      position: relative;

      & > cra-icon {
        height: 20px;
        width: 20px;
      }

      & > cra-icon-button {
        position: absolute;
        right: 0;
      }

      & > cros-badge {
        background-color: var(--cros-sys-complement);
        color: var(--cros-sys-on_surface);
        margin: 0;
      }

      & > md-circular-progress {
        --md-circular-progress-size: 24px;

        margin: -2px;
      }

      & > .progress {
        font: var(--cros-annotation-1-font);
        margin: 0 4px 0 auto;
      }
    }

    #main {
      display: flex;
      flex-flow: column;
      gap: 8px;

      /* For the thumb up/down buttons. */
      min-height: 48px;
      position: relative;
      z-index: 0;

      /* TODO: b/336963138 - Transition on height on child change. */

      &:not(.open) {
        display: none;
      }

      & > genai-placeholder {
        margin: 12px;
      }
    }

    #summary {
      font: var(--cros-body-1-font);
      padding: 12px 16px;
      white-space: pre-wrap;
    }

    #footer {
      font: var(--cros-annotation-2-font);
      padding: 0 82px 12px 12px;

      & > a,
      & > a:visited {
        color: inherit;
      }
    }

    genai-feedback-buttons {
      --background-color: var(--cros-sys-app_base);

      bottom: 0;
      position: absolute;
      right: 0;
    }

    #disabled {
      background: var(--cros-sys-surface_variant);
      border-radius: 8px;
      color: var(--cros-sys-on_surface_variant);
      font: var(--cros-label-1-font);
      padding: 8px;
      text-align: center;
    }
  `;

  static override properties: PropertyDeclarations = {
    transcription: {attribute: false},
  };

  transcription: Transcription|null = null;

  // TODO(pihsun): Store the summarization in metadata.
  // TODO(pihsun): Reset summarization when textTokens changes? Probably
  // should just pass in the whole metadata after we have summarization in
  // metadata though, but would still need a way to "re-run" summarization
  // for dev iteration purpose.
  private readonly summary = signal<ModelResponse<string>|null>(null);

  // TODO(pihsun): Have a single struct for all possible states, instead of
  // multiple boolean.
  private readonly summaryRequested = signal(false);

  // TODO(pihsun): Animation when open.
  private readonly summaryOpened = signal(false);

  private readonly platformHandler = usePlatformHandler();

  private onSummaryOpenClick(ev: MouseEvent) {
    ev.stopPropagation();
    if (!this.summaryRequested.value) {
      // TODO(pihsun): Better handling for promise.
      void this.requestSummary();
    } else {
      this.summaryOpened.value = !this.summaryOpened.value;
    }
  }

  private async requestSummary() {
    this.summaryRequested.value = true;
    this.summaryOpened.value = true;
    const text = this.transcription?.toPlainText() ?? '';
    const model = await this.platformHandler.summaryModelLoader.load();
    try {
      this.summary.value = await model.execute(text);
      // TODO(pihsun): Handle error.
    } finally {
      model.close();
    }
  }

  private renderSummaryFooter() {
    return html`
      <div id="footer">
        ${i18n.genAiDisclaimerText}
        <!-- TODO: b/336963138 - Add correct link -->
        <a href="javascript:;">${i18n.genAiLearnMoreLink}</a>
      </div>
      <genai-feedback-buttons></genai-feedback-buttons>
    `;
  }

  private renderSummaryContent() {
    const summary = this.summary.value;
    if (summary === null) {
      return html`<genai-placeholder></genai-placeholder>`;
    }
    switch (summary.kind) {
      case 'error':
        return html`<genai-error
          .error=${summary.error}
          .resultType=${GenaiResultType.SUMMARY}
        >
        </genai-error>`;
      case 'success':
        return html`<div id="summary">${summary.result}</div>
          ${this.renderSummaryFooter()}`;
      default:
        assertExhaustive(summary);
    }
  }

  private renderSummary() {
    const classes = {
      open: this.summaryOpened.value,
    };
    const expandIconName =
      this.summaryOpened.value ? 'chevron_up' : 'chevron_down';

    // TODO: b/336963138 - Implement error state.
    return html`
      <div id="container">
        <div id="header" class="sheet">
          <cra-icon name="summarize_auto"></cra-icon>
          <span>${i18n.summaryHeader}</span>
          <cros-badge>${i18n.genAiExperimentBadge}</cros-badge>
          <cra-icon-button
            @click=${this.onSummaryOpenClick}
            buttonstyle="floating"
          >
            <cra-icon name=${expandIconName} slot="icon"></cra-icon>
          </cra-icon-button>
        </div>
        <div id="main" class="sheet ${classMap(classes)}">
          ${this.renderSummaryContent()}
        </div>
      </div>
    `;
  }

  private renderSummaryInstalling(progress: number) {
    return html`
      <div id="container">
        <div id="header" class="sheet">
          <cra-icon name="summarize_auto"></cra-icon>
          <span>${i18n.summaryHeader}</span>
          <cros-badge>${i18n.genAiExperimentBadge}</cros-badge>
          <span class="progress">
            ${i18n.summaryDownloadingProgressDescription(progress)}
          </span>
          <md-circular-progress indeterminate></md-circular-progress>
        </div>
      </div>
    `;
  }

  override render(): RenderResult {
    const summaryModelState = this.platformHandler.summaryModelLoader.state;
    const summaryEnabled = settings.value.summaryEnabled;

    if (summaryModelState.value.kind === 'unavailable') {
      this.classList.add('empty');
      return nothing;
    }

    this.classList.remove('empty');
    switch (summaryEnabled) {
      case SummaryEnableState.DISABLED:
        return html`<div id="disabled">${i18n.summaryDisabledLabel}</div>`;
      case SummaryEnableState.UNKNOWN:
        return html`<summary-consent-card></summary-consent-card>`;
      case SummaryEnableState.ENABLED:
        switch (summaryModelState.value.kind) {
          case 'error':
            // TODO(pihsun): Handle error
            return nothing;
          case 'installing':
            return this.renderSummaryInstalling(
              summaryModelState.value.progress,
            );
          case 'installed':
            return this.renderSummary();
          case 'notInstalled':
            return html`<summary-consent-card></summary-consent-card>`;
          default:
            assertExhaustive(summaryModelState.value.kind);
        }
      // eslint doesn't detect that the above case never reaches here, but tsc
      // prevents us from adding "break;" here since it's unreachable code.
      // eslint-disable-next-line no-fallthrough
      default:
        assertExhaustive(summaryEnabled);
    }
  }
}

window.customElements.define('summarization-view', SummarizationView);

declare global {
  interface HTMLElementTagNameMap {
    'summarization-view': SummarizationView;
  }
}
