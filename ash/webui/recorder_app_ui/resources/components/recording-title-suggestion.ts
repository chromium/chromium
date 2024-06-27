// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/badge/badge.js';
import 'chrome://resources/cros_components/chip/chip.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './genai-placeholder.js';

import {Chip} from 'chrome://resources/cros_components/chip/chip.js';
import {
  css,
  html,
  map,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {
  ComputedState,
  ReactiveLitElement,
  ScopedAsyncComputed,
} from '../core/reactive/lit.js';
import {assertInstanceof} from '../core/utils/assert.js';

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
      font: var(--cros-annotation-2-font);
      padding: 12px 16px;

      & > a,
      & > a:visited {
        color: inherit;
      }
    }

    #feedback-buttons {
      background-color: var(--cros-sys-app_base_shaded);
      border-radius: 18px 0 0;
      display: flex;
      flex-flow: row;
      gap: 8px;
      height: 24px;
      padding: 4px 4px 0 8px;
      position: absolute;
      position-anchor: --footer;
      inset-area: top span-left;

      & > cra-icon-button {
        margin: 0;
      }

      & > svg {
        color: var(--cros-sys-app_base_shaded);
        position: absolute;
        z-index: -1;
      }

      & > .top-right {
        right: 0;
        top: -10px;
      }

      & > .bottom-left {
        bottom: 0;
        left: -10px;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    suggestedTitles: {attribute: false},
  };

  suggestedTitles: ScopedAsyncComputed<string[]|null>|null = null;

  private onCloseClick() {
    this.dispatchEvent(new Event('close'));
  }

  private onSuggestionClick(ev: PointerEvent) {
    const target = assertInstanceof(ev.target, Chip);
    this.dispatchEvent(new CustomEvent('change', {detail: target.label}));
  }

  private renderSuggestion(suggestion: string) {
    // TODO: b/336963138 - Handle when the suggestion is too long to fit in one
    // line.
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
        <a href="#">${i18n.genAiLearnMoreLink}</a>
      </div>
      <div id="feedback-buttons">
        <!-- These are the two additional "rounded corner". -->
        <svg class="top-right" width="10" height="10">
          <path d="M 10 10 H 0 a 10 10 0 0 0 10 -10 V 10" fill="currentcolor" />
        </svg>
        <svg class="bottom-left" width="10" height="10">
          <path d="M 10 10 H 0 a 10 10 0 0 0 10 -10 V 10" fill="currentcolor" />
        </svg>
        <!-- TODO: b/336963138 - implements thumbs up / down -->
        <cra-icon-button buttonstyle="toggle" size="small">
          <cra-icon name="thumb_up" slot="icon"></cra-icon>
          <cra-icon name="thumb_up_filled" slot="selectedIcon"></cra-icon>
        </cra-icon-button>
        <cra-icon-button buttonstyle="toggle" size="small">
          <cra-icon name="thumb_down" slot="icon"></cra-icon>
          <cra-icon name="thumb_down_filled" slot="selectedIcon"></cra-icon>
        </cra-icon-button>
      </div>
    `;
  }

  private renderContent() {
    if (this.suggestedTitles === null ||
        this.suggestedTitles.state !== ComputedState.DONE ||
        this.suggestedTitles.value === null) {
      // TOOD(pihsun): Handler error.
      return html`<div id="loading">
        <genai-placeholder></genai-placeholder>
      </div>`;
    }
    const suggestions = map(
        this.suggestedTitles.value,
        (s) => this.renderSuggestion(s),
    );
    return html`<div id="suggestions">${suggestions}</div>
      ${this.renderSuggestionFooter()}`;
  }

  override render(): RenderResult {
    return html`
      <div id="header">
        <span>${i18n.titleGenerationHeader}</span>
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
