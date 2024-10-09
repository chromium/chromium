// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/accordion/accordion.js';
import 'chrome://resources/cros_components/accordion/accordion_item.js';
import 'chrome://resources/cros_components/badge/badge.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './genai-error.js';
import './genai-feedback-buttons.js';
import './genai-placeholder.js';
import './spoken-message.js';
import './summary-consent-card.js';

import {
  createRef,
  css,
  CSSResultGroup,
  html,
  map,
  nothing,
  PropertyDeclarations,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {
  GenaiResultType,
  ModelResponse,
} from '../core/on_device_model/types.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {Transcription} from '../core/soda/soda.js';
import {settings, SummaryEnableState} from '../core/state/settings.js';
import {HELP_URL} from '../core/url_constants.js';
import {
  assert,
  assertExhaustive,
  assertExists,
} from '../core/utils/assert.js';

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

    cros-accordion::part(card) {
      --cros-card-border-color: none;

      width: initial;
    }

    cros-accordion-item {
      --cros-accordion-item-leading-padding-inline-end: 4px;

      &::part(row) {
        background-color: var(--cros-sys-app_base_shaded);
        border-radius: 4px;
      }

      &::part(content) {
        margin: 4px 0 0;
        padding: 0;
      }

      &[disabled] {
        opacity: 1;
      }

      & > cra-icon {
        height: 20px;
        width: 20px;
      }

      & > [slot="title"] {
        align-items: center;
        display: flex;
        flex-flow: row;

        & > span {
          font: var(--cros-button-1-font);
        }

        & > cros-badge {
          background-color: var(--cros-sys-complement);
          color: var(--cros-sys-on_surface);
          margin: 0 0 0 4px;
        }

        & > .progress {
          font: var(--cros-annotation-1-font);
          margin: 0 4px 0 auto;
        }
      }
    }

    #main {
      background-color: var(--cros-sys-app_base_shaded);
      border-radius: 4px;
      display: flex;
      flex-flow: column;
      gap: 8px;

      /* For the thumb up/down buttons. */
      min-height: 48px;
      position: relative;
      z-index: 0;

      /* TODO: b/336963138 - Transition on height on child change. */
      & > genai-placeholder {
        margin: 12px;
      }
    }

    #summary {
      font: var(--cros-body-1-font);
      list-style-position: outside;
      margin: 12px 12px 12px 22px;
      padding: 0 0 0 10px;
    }

    #footer {
      font: var(--cros-annotation-2-font);
      padding: 0 96px 12px 12px;

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

  private readonly summaryContainer = createRef<HTMLDivElement>();

  private readonly downloadRequested = signal(false);

  private readonly downloadPerfCollected = signal(false);

  get summaryContainerForTest(): HTMLDivElement {
    return assertExists(this.summaryContainer.value);
  }

  getSummaryContentForTest(): string {
    const summary = assertExists(
      this.summary.value,
      'Summary is still processing',
    );
    assert(summary.kind === 'success', `Summary status is ${summary.kind}`);
    return summary.result;
  }

  override updated(): void {
    const summaryState = this.platformHandler.summaryModelLoader.state;
    if (settings.value.summaryEnabled === SummaryEnableState.ENABLED &&
        summaryState.value.kind === 'installing') {
      this.downloadRequested.value = true;
    } else if (this.downloadRequested.value &&
               !this.downloadPerfCollected.value &&
               summaryState.value.kind === 'installed') {
      // TODO: b/367263595 - Collect perf in PlatformHandler instead.
      this.platformHandler.perfLogger.finish('summaryModelDownload');
      this.downloadPerfCollected.value = true;
    }
  }

  private async requestSummary() {
    this.summaryRequested.value = true;
    this.summaryOpened.value = true;

    this.platformHandler.perfLogger.start({
      kind: 'summary',
      wordCount: this.transcription?.wordCount ?? 0,
    });

    const text = this.transcription?.toPlainText() ?? '';
    this.summary.value =
      await this.platformHandler.summaryModelLoader.loadAndExecute(text);
    this.sendSummarizeEvent();
    this.platformHandler.perfLogger.finish('summary');
  }

  private sendSummarizeEvent() {
    const response = this.summary.value;
    if (this.transcription === null || response === null) {
      return;
    }

    this.platformHandler.eventsSender.sendSummarizeEvent({
      responseError: response.kind === 'error' ? response.error : null,
      wordCount: this.transcription.wordCount,
    });
  }

  private renderSummaryFooter() {
    return html`
      <div id="footer">
        ${i18n.genAiDisclaimerText}
        <a
          href=${HELP_URL}
          target="_blank"
          aria-label=${i18n.genAiLearnMoreLinkTooltip}
        >
          ${i18n.genAiLearnMoreLink}
        </a>
      </div>
      <genai-feedback-buttons .resultType=${GenaiResultType.SUMMARY}>
      </genai-feedback-buttons>
    `;
  }

  private renderSummaryResult(result: string) {
    const sentences = result.split('\n');
    return map(sentences, (sentence) => {
      // Remove the leading hyphen and space from the sentence, if any.
      return html`<li>${sentence.replace(/^-\s+/, '')}</li>`;
    });
  }

  private renderSummaryContent() {
    const summary = this.summary.value;
    if (summary === null) {
      return html`
        <genai-placeholder
          aria-label=${i18n.summaryStartedStatusMessage}
          aria-live="polite"
          tabindex="-1"
        ></genai-placeholder>
      `;
    }
    switch (summary.kind) {
      case 'error':
        return html`<spoken-message role="status" aria-live="polite">
            ${i18n.summaryFailedStatusMessage}
          </spoken-message>
          <genai-error
            .error=${summary.error}
            .resultType=${GenaiResultType.SUMMARY}
          >
          </genai-error>`;
      case 'success':
        return html`<spoken-message role="status" aria-live="polite">
            ${i18n.summaryFinishedStatusMessage}
          </spoken-message>
          <ul id="summary" ${ref(this.summaryContainer)}>
            ${this.renderSummaryResult(summary.result)}
          </ul>
          ${this.renderSummaryFooter()}`;
      default:
        assertExhaustive(summary);
    }
  }

  private onSummaryExpanded() {
    if (!this.summaryRequested.value) {
      // TODO(pihsun): Better handling for promise.
      void this.requestSummary();
    } else {
      this.summaryOpened.value = true;
    }
  }

  private onSummaryCollapsed() {
    this.summaryOpened.value = false;
  }

  private renderSummary() {
    // TODO: b/336963138 - Implement error state.
    const downloadStatus = html`<spoken-message
      role="status"
      aria-live="polite"
    >
      ${i18n.summaryDownloadFinishedStatusMessage}
    </spoken-message>`;
    return html`
      <cros-accordion variant="compact">
        <cros-accordion-item
          @cros-accordion-item-expanded=${this.onSummaryExpanded}
          @cros-accordion-item-collapsed=${this.onSummaryCollapsed}
        >
          <cra-icon name="summarize_auto" slot="leading"></cra-icon>
          <div slot="title">
            <span>${i18n.summaryHeader}</span>
            <cros-badge>${i18n.genAiExperimentBadge}</cros-badge>
          </div>
          <div id="main">${this.renderSummaryContent()}</div>
        </cros-accordion-item>
      </cros-accordion>
      ${this.downloadRequested.value ? downloadStatus : nothing}
    `;
  }

  private renderSummaryInstalling(progress: number) {
    return html`
      <cros-accordion
        variant="compact"
        aria-label=${i18n.summaryDownloadStartedStatusMessage}
        aria-live="polite"
        role="status"
      >
        <cros-accordion-item disabled>
          <cra-icon name="summarize_auto" slot="leading"></cra-icon>
          <div slot="title">
            <span>${i18n.summaryHeader}</span>
            <cros-badge>${i18n.genAiExperimentBadge}</cros-badge>

            <span class="progress">
              ${i18n.summaryDownloadingProgressDescription(progress)}
            </span>
          </div>
        </cros-accordion-item>
      </cros-accordion>
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
