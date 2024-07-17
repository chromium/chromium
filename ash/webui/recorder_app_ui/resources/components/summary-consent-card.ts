// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-icon.js';
import './cra/cra-button.js';

import {css, CSSResultGroup, html} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ModelId} from '../core/on_device_model/types.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {settings, SummaryEnableState} from '../core/state/settings.js';

export class SummaryConsentCard extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    :host {
      display: block;
    }

    #container {
      align-items: flex-start;
      background-color: var(--cros-sys-app_base_shaded);
      border-radius: 12px;
      display: flex;
      flex-flow: row;
      gap: 16px;
      padding: 16px;

      & > cra-icon {
        background-color: var(--cros-sys-base_elevated);
        border-radius: 12px;
        height: 24px;
        padding: 8px;
        width: 24px;
      }
    }

    #main {
      align-items: stretch;
      display: flex;
      flex: 1;
      flex-flow: column;
    }

    #header {
      color: var(--cros-sys-on_surface);
      font: var(--cros-button-1-font);
    }

    #description {
      color: var(--cros-sys-on_surface_variant);
      font: var(--cros-body-2-font);
      margin-top: 8px;
    }

    #actions {
      align-self: flex-end;
      display: flex;
      flex-flow: row;
      gap: 8px;
      margin-top: 16px;
    }
  `;

  private readonly platformHandler = usePlatformHandler();

  private onDownloadClick() {
    // TODO(pihsun): This is the same logic as "onDownloadSummaryClick" in
    // settings-menu.ts, consider how to consolidate the logic at one place.
    settings.mutate((s) => {
      s.summaryEnabled = SummaryEnableState.ENABLED;
    });
    this.platformHandler.downloadModel(ModelId.SUMMARY);
    // TODO(pihsun): This downloads the model used by title suggestion, and
    // both model are downloaded together with the same toggle. This currently
    // doesn't take extra time because it's the base model for summary. Change
    // this to title suggestion specific model when we have LoRA for that.
    this.platformHandler.downloadModel(ModelId.GEMINI_XXS_IT_BASE);
  }

  private onDisableClick() {
    settings.mutate((s) => {
      s.summaryEnabled = SummaryEnableState.DISABLED;
    });
  }

  override render(): RenderResult {
    return html`<div id="container">
      <cra-icon name="summarize_auto"></cra-icon>
      <div id="main">
        <span id="header">${i18n.summaryDownloadModelHeader}</span>
        <span id="description">${i18n.summaryDownloadModelDescription}</span>
        <div id="actions">
          <cra-button
            .label=${i18n.summaryDownloadModelDisableButton}
            button-style="floating"
            @click=${this.onDisableClick}
          >
          </cra-button>
          <cra-button
            .label=${i18n.summaryDownloadModelDownloadButton}
            @click=${this.onDownloadClick}
          >
          </cra-button>
        </div>
      </div>
    </div>`;
  }
}

window.customElements.define('summary-consent-card', SummaryConsentCard);

declare global {
  interface HTMLElementTagNameMap {
    'summary-consent-card': SummaryConsentCard;
  }
}
