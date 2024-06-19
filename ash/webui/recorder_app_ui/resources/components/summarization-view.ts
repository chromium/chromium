// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/badge/badge.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './genai-placeholder.js';

import {
  classMap,
  css,
  CSSResultGroup,
  html,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ModelId} from '../core/platform_handler.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {concatTextTokens, TextToken} from '../core/soda/soda.js';

export class SummarizationView extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    :host {
      display: block;
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
      font: var(--cros-body-2-font);
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

    #feedback-buttons {
      background-color: var(--cros-sys-app_base);
      border-radius: 18px 0 0;
      bottom: 0;
      display: flex;
      flex-flow: row;
      gap: 8px;
      padding: 4px 4px 0 8px;
      position: absolute;
      right: 0;

      & > cra-icon-button {
        margin: 0;
      }

      & > svg {
        color: var(--cros-sys-app_base);
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
    textTokens: {attribute: false},
  };

  textTokens: TextToken[] = [];

  // TODO(pihsun): Store the summarization in metadata.
  // TODO(pihsun): Reset summarization when textTokens changes? Probably
  // should just pass in the whole metadata after we have summarization in
  // metadata though, but would still need a way to "re-run" summarization
  // for dev iteration purpose.
  private readonly summary = signal<string|null>(null);

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
    const text = concatTextTokens(this.textTokens);
    const model = await this.platformHandler.loadModel(ModelId.SUMMARY);
    try {
      this.summary.value = await model.summarize(text);
      // TODO(pihsun): Handle error.
    } finally {
      model.close();
    }
  }

  private renderSummaryFooter() {
    const genAiDisclaimer = i18n(
        'Generative AI is experimental and content may be inaccurate, ' +
            'misleading, or offensive.',
    );
    return html`
      <div id="footer">
        ${genAiDisclaimer}
        <!-- TODO: b/336963138 - Add correct link -->
        <a href="#">${i18n('Learn more')}</a>
      </div>
      <div id="feedback-buttons">
        <!-- These are the two additional "white rounded corner". -->
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

  private renderSummary() {
    if (this.summary.value === null) {
      return html`<genai-placeholder></genai-placeholder>`;
    }
    return html`<div id="summary">${this.summary.value}</div>
      ${this.renderSummaryFooter()}`;
  }

  override render(): RenderResult {
    const classes = {
      open: this.summaryOpened.value,
    };

    // TODO: b/336963138 - Implement consent UI / download UI / ...
    return html`
      <div id="container">
        <div id="header" class="sheet">
          <cra-icon name="summarize_auto"></cra-icon>
          <span>${i18n('Summary')}</span>
          <cros-badge>${i18n('Experiment')}</cros-badge>
          <cra-icon-button
            @click=${this.onSummaryOpenClick}
            buttonstyle="floating"
          >
            <cra-icon
              name=${this.summaryOpened.value ? 'chevron_up' : 'chevron_down'}
              slot="icon"
            ></cra-icon>
          </cra-icon-button>
        </div>
        <div id="main" class="sheet ${classMap(classes)}">
          ${this.renderSummary()}
        </div>
      </div>
    `;
  }
}

window.customElements.define('summarization-view', SummarizationView);

declare global {
  interface HTMLElementTagNameMap {
    'summarization-view': SummarizationView;
  }
}
