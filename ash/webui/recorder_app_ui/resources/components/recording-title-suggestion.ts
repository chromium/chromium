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

import {Chip} from 'chrome://resources/cros_components/chip/chip.js';
import {
  css,
  html,
  map,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ModelResponse} from '../core/on_device_model/types.js';
import {
  ComputedState,
  ReactiveLitElement,
  ScopedAsyncComputed,
} from '../core/reactive/lit.js';
import {assertExhaustive, assertInstanceof} from '../core/utils/assert.js';

import {GenaiResultType} from './genai-error.js';

/**
 * The title suggestion popup in playback page of Recorder App.
 */
export class RecordingTitleSuggestion extends ReactiveLitElement {
  static override styles = css`
    :host {
      background-color: var(--cros-sys-base_elevated);
      border-radius: 12px;
      box-shadow: var(--cros-sys-app_elevation3);
      display: block;

      /* To have the border-radius applied to content. */
      overflow: hidden;
      z-index: 30;
    }

    #header {
      align-items: flex-end;
      display: flex;
      flex-flow: row;
      font: var(--cros-title-1-font);
      justify-content: space-between;
      padding: 4px 4px 0 16px;
      position: relative;

      & > cra-icon-button {
        margin: 0;
      }
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
      inset-area: top span-left;
      position: absolute;
      position-anchor: --footer;
      z-index: 1;

      &::part(bottom-left-corner) {
        bottom: 8px;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    suggestedTitles: {attribute: false},
  };

  suggestedTitles: ScopedAsyncComputed<ModelResponse<string[]>|null>|null =
    null;

  private onCloseClick() {
    this.dispatchEvent(new Event('close'));
  }

  private onSuggestionClick(ev: PointerEvent) {
    const target = assertInstanceof(ev.target, Chip);
    this.dispatchEvent(new CustomEvent('change', {detail: target.label}));
  }

  private renderSuggestion(suggestion: string) {
    // TODO: b/336963138 - Handle when the suggestion is too long to fit in one
    // line. Currently the cros-chip (and underlying md-chip) can't handle
    // either multiline or setting width / text-overflow: ellipsis, so we might
    // need to change to use our own component.
    return html`<cros-chip
      type="input"
      label=${suggestion}
      class="suggestion"
      @click=${this.onSuggestionClick}
    ></cros-chip>`;
  }

  private renderSuggestionFooter() {
    return html`
      <div id="footer">
        ${i18n.genAiDisclaimerText}
        <!-- TODO: b/336963138 - Add correct link -->
        <a href="javascript:;">${i18n.genAiLearnMoreLink}</a>
      </div>
      <genai-feedback-buttons></genai-feedback-buttons>
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
        <genai-placeholder></genai-placeholder>
      </div>`;
    }
    const suggestedTitles = this.suggestedTitles.value;
    switch (suggestedTitles.kind) {
      case 'error': {
        return html`<genai-error
          .error=${suggestedTitles.error}
          .resultType=${GenaiResultType.TITLE_SUGGESTION}
        ></genai-error>`;
      }
      case 'success': {
        const suggestions = map(
          suggestedTitles.result,
          (s) => this.renderSuggestion(s),
        );
        return html`<div id="suggestions">${suggestions}</div>
          ${this.renderSuggestionFooter()}`;
      }
      default:
        assertExhaustive(suggestedTitles);
    }
  }

  override render(): RenderResult {
    return html`
      <div id="header">
        <span>${i18n.titleSuggestionHeader}</span>
        <cra-icon-button
          buttonstyle="floating"
          size="small"
          shape="circle"
          @click=${this.onCloseClick}
        >
          <cra-icon slot="icon" name="close"></cra-icon>
        </cra-icon-button>
      </div>
      ${this.renderContent()}
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
