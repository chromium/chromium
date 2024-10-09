// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/badge/badge.js';
import 'chrome://resources/cros_components/chip/chip.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './genai-error.js';
import './genai-feedback-buttons.js';
import './genai-placeholder.js';
import './spoken-message.js';

import {Chip} from 'chrome://resources/cros_components/chip/chip.js';
import {
  createRef,
  css,
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
import {
  ComputedState,
  ReactiveLitElement,
  ScopedAsyncComputed,
} from '../core/reactive/lit.js';
import {HELP_URL} from '../core/url_constants.js';
import {
  assertExhaustive,
  assertExists,
  assertInstanceof,
} from '../core/utils/assert.js';

import {CraIconButton} from './cra/cra-icon-button.js';

/**
 * The title suggestion popup in playback page of Recorder App.
 */
export class RecordingTitleSuggestion extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
      z-index: 10;
    }

    #container {
      background-color: var(--cros-sys-base_elevated);
      border-radius: 12px;
      box-shadow: var(--cros-sys-app_elevation3);

      /* To have the border-radius applied to content. */
      overflow: hidden;

      & > cra-icon-button {
        position: absolute;
        right: 4px;
        top: 4px;
      }
    }

    #header {
      font: var(--cros-title-1-font);
      padding: 16px 8px 0 16px;
    }

    #loading {
      padding: 16px;

      & > genai-placeholder::part(line-4) {
        display: none;
      }
    }

    #suggestions {
      align-items: flex-start;
      display: flex;
      flex-flow: column;
      gap: 8px;
      padding: 16px 16px 24px;

      & > cros-chip {
        max-width: 100%;
      }
    }

    #footer {
      anchor-name: --footer;
      background-color: var(--cros-sys-app_base_shaded);
      box-sizing: border-box;
      font: var(--cros-annotation-2-font);

      /*
       * min-width and width are to avoid the footer from expanding the parent
       * width.
       */
      min-width: 100%;
      padding: 12px 16px;
      width: 0;

      & > a,
      & > a:visited {
        color: inherit;
      }
    }

    genai-feedback-buttons {
      --background-color: var(--cros-sys-app_base_shaded);

      bottom: -8px;
      position: absolute;
      position-anchor: --footer;
      position-area: top span-left;
      z-index: 1;

      &::part(bottom-left-corner) {
        bottom: 8px;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    suggestedTitles: {attribute: false},
    wordCount: {attribute: false},
  };

  suggestedTitles: ScopedAsyncComputed<ModelResponse<string[]>|null>|null =
    null;

  wordCount = 0;

  private readonly closeButtonRef = createRef<CraIconButton>();

  private readonly platformHandler = usePlatformHandler();

  get firstSuggestedTitleForTest(): Chip {
    return assertExists(this.shadowRoot?.querySelector('.suggestion'));
  }

  nthSuggestedTitleForTest(index: number): Chip {
    const allSuggestions = assertExists(
      this.shadowRoot?.querySelectorAll('.suggestion'),
    );
    return assertInstanceof(allSuggestions[index], Chip);
  }

  override firstUpdated(): void {
    // Automatically focus on the close button when the dialog is opened to make
    // screen reader speak the content in the dialog.
    const closeButton = assertExists(this.closeButtonRef.value);
    closeButton.updateComplete.then(() => {
      closeButton.focus();
    });
  }

  private sendSuggestTitleEvent(
    suggestionAccepted: boolean,
    acceptedIndex = -1,
  ) {
    // Skip sending the event if there is no transcription or suggested titles.
    const suggestedTitle = this.suggestedTitles?.value;
    if (suggestedTitle === null || suggestedTitle === undefined) {
      return;
    }

    const hasError = suggestedTitle.kind === 'error';
    this.platformHandler.eventsSender.sendSuggestTitleEvent({
      acceptedSuggestionIndex: acceptedIndex,
      responseError: hasError ? suggestedTitle.error : null,
      suggestionAccepted,
      wordCount: this.wordCount,
    });
  }

  private onCloseClick() {
    this.dispatchEvent(new Event('close'));
    this.sendSuggestTitleEvent(/* suggestionAccepted= */ false);
  }

  private onSuggestionClick(label: string, index: number) {
    this.dispatchEvent(new CustomEvent('change', {detail: label}));
    this.sendSuggestTitleEvent(/* suggestionAccepted= */ true, index);
  }

  private renderSuggestion(suggestion: string, index: number) {
    return html`<cros-chip
      show-tooltip-when-truncated
      type="input"
      label=${suggestion}
      class="suggestion"
      @click=${() => this.onSuggestionClick(suggestion, index)}
    ></cros-chip>`;
  }

  private renderSuggestionFooter() {
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
      <genai-feedback-buttons .resultType=${GenaiResultType.TITLE_SUGGESTION}>
      </genai-feedback-buttons>
    `;
  }

  private renderContent() {
    // TODO(pihsun): There should also be a consent / download model / loading
    // state for title suggestion too. Implement it when the UI spec is done.
    if (this.suggestedTitles === null ||
        this.suggestedTitles.state !== ComputedState.DONE ||
        this.suggestedTitles.value === null) {
      // TOOD(pihsun): Handle error.
      return html`<div id="loading">
        <genai-placeholder
          aria-label=${i18n.titleSuggestionStartedStatusMessage}
          aria-live="polite"
          role="status"
          tabindex="-1"
        ></genai-placeholder>
      </div>`;
    }
    const suggestedTitles = this.suggestedTitles.value;
    switch (suggestedTitles.kind) {
      case 'error': {
        return html`<spoken-message role="status" aria-live="polite">
            ${i18n.titleSuggestionFailedStatusMessage}
          </spoken-message>
          <genai-error
            .error=${suggestedTitles.error}
            .resultType=${GenaiResultType.TITLE_SUGGESTION}
          ></genai-error>`;
      }
      case 'success': {
        const suggestions = map(
          suggestedTitles.result,
          (s, index) => this.renderSuggestion(s, index),
        );
        return html`<spoken-message role="status" aria-live="polite">
            ${i18n.titleSuggestionFinishedStatusMessage}
          </spoken-message>
          <div id="suggestions">${suggestions}</div>
          ${this.renderSuggestionFooter()}`;
      }
      default:
        assertExhaustive(suggestedTitles);
    }
  }

  private renderHeader(): RenderResult {
    if (this.suggestedTitles?.value?.kind === 'error') {
      return nothing;
    }
    return html`<div id="header">${i18n.titleSuggestionHeader}</div>`;
  }

  override render(): RenderResult {
    return html`
      <div
        id="container"
        role="dialog"
        aria-label=${i18n.titleSuggestionHeader}
      >
        ${this.renderHeader()}
        <cra-icon-button
          buttonstyle="floating"
          size="small"
          shape="circle"
          @click=${this.onCloseClick}
          aria-label=${i18n.closeDialogButtonTooltip}
          ${ref(this.closeButtonRef)}
        >
          <cra-icon slot="icon" name="close"></cra-icon>
        </cra-icon-button>
        ${this.renderContent()}
      </div>
    `;
  }
}

window.customElements.define(
  'recording-title-suggestion',
  RecordingTitleSuggestion,
);

declare global {
  interface HTMLElementTagNameMap {
    'recording-title-suggestion': RecordingTitleSuggestion;
  }
}
