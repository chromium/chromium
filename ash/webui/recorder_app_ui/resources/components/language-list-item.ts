// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mwc/@material/web/focus/md-focus-ring.js';
import 'chrome://resources/mwc/@material/web/progress/circular-progress.js';
import './cra/cra-button.js';
import './cra/cra-icon.js';
import './settings-row.js';

import {
  css,
  html,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ModelState} from '../core/on_device_model/types.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {LangPackInfo} from '../core/soda/language_info.js';
import {
  assertExhaustive,
  assertNotReached,
} from '../core/utils/assert.js';
import {stopPropagation, suppressEvent} from '../core/utils/event_handler.js';

/**
 * An item in the language selection list.
 */
export class LanguageListItem extends ReactiveLitElement {
  static override styles = css`
    md-focus-ring {
      --md-focus-ring-outward-offset: -8px;
      --md-focus-ring-shape: 20px;
    }

    #root {
      outline: none;
      position: relative;
    }

    /*
     * TODO: b/377885042 - Move the circular progress to a separate component.
     */
    settings-row cra-button md-circular-progress {
      --md-circular-progress-active-indicator-color: var(--cros-sys-disabled);

      /*
       * This has a lower precedence than the size override in cros-button,
       * but still need to be set to have correct line width.
       */
      --md-circular-progress-size: 24px;

      /*
       * This is to override the size setting for slotted element in
       * cros-button. On figma the circular progress have 2px padding, but
       * md-circular-progres has a non-configurable 4px padding. Setting a
       * negative margin so the extra padding doesn't expand the button size.
       */
      height: 24px;
      margin: -2px;
      width: 24px;
    }
  `;

  static override properties: PropertyDeclarations = {
    langPackInfo: {attribute: false},
    selected: {type: Boolean},
    sodaState: {attribute: false},
  };

  langPackInfo: LangPackInfo|null = null;

  selected = false;

  sodaState: ModelState = {kind: 'unavailable'};

  private onDownload() {
    if (this.langPackInfo === null) {
      return;
    }
    this.dispatchEvent(new CustomEvent('language-download-click', {
      detail: this.langPackInfo.languageCode,
      bubbles: true,
      composed: true,
    }));
  }

  private onSelect() {
    if (this.langPackInfo === null) {
      return;
    }
    this.dispatchEvent(new CustomEvent('language-select-click', {
      detail: this.langPackInfo.languageCode,
      bubbles: true,
      composed: true,
    }));
  }

  private activateRow() {
    this.renderRoot.querySelector('settings-row')?.click();
  }

  private onKeyUp(ev: KeyboardEvent) {
    if (ev.key === ' ') {
      suppressEvent(ev);
      this.activateRow();
    }
  }

  private onKeyDown(ev: KeyboardEvent) {
    if (ev.key === ' ') {
      // Prevents page from scroll down.
      ev.preventDefault();
    }
    if (ev.key === 'Enter') {
      suppressEvent(ev);
      this.activateRow();
    }
  }

  private renderDescriptionAndAction(): RenderResult {
    const downloadButton = html`
      <cra-button
        slot="action"
        button-style="secondary"
        tabindex="-1"
        .label=${i18n.languagePickerLanguageDownloadButton}
        @keyup=${stopPropagation}
        @keydown=${stopPropagation}
        @click=${this.onDownload}
      ></cra-button>
    `;

    const kind = this.sodaState.kind;
    switch (kind) {
      case 'notInstalled':
        return downloadButton;
      case 'error':
        // Shows the download button for users to try again.
        return html`
          <span slot="description" class="error">
            ${i18n.languagePickerLanguageErrorDescription}
          </span>
          ${downloadButton}
        `;
      case 'installing': {
        const progressDescription =
          i18n.languagePickerLanguageDownloadingProgressDescription(
            this.sodaState.progress,
          );
        return html`
          <span slot="description">${progressDescription}</span>
          <cra-button
            slot="action"
            button-style="secondary"
            .label=${i18n.languagePickerLanguageDownloadingButton}
            disabled
          >
            <md-circular-progress indeterminate slot="leading-icon">
            </md-circular-progress>
          </cra-button>
        `;
      }
      case 'installed':
        if (this.selected) {
          // Row is not interactive when the language is selected.
          return html`
            <cra-icon slot="action" name="checked" disabled></cra-icon>
          `;
        }
        return html`<span slot="action" @click=${this.onSelect}></span>`;
      case 'unavailable':
        return assertNotReached('SODA unavailable but the row is rendered.');
      default:
        return assertExhaustive(kind);
    }
  }

  private isFocusable() {
    if (this.sodaState.kind === 'installing' ||
        (this.sodaState.kind === 'installed' && this.selected)) {
      return false;
    }
    return true;
  }

  override render(): RenderResult {
    if (this.langPackInfo === null || this.sodaState.kind === 'unavailable') {
      return nothing;
    }
    // TODO: b/384418702 - Add aria label of each state in #root and set
    // settings-row aria-hidden to `true` to avoid redundant announcement by
    // screen reader.
    return html`
      <div id="root"
        tabindex=${this.isFocusable() ? 0 : -1}
        @click=${this.activateRow}
        @keydown=${this.onKeyDown}
        @keyup=${this.onKeyUp}
      >
        <settings-row>
          <span slot="label">${this.langPackInfo.displayName}</span>
          ${this.renderDescriptionAndAction()}
        </settings-row>
        ${this.isFocusable() ? html`<md-focus-ring></md-focus-ring>` : nothing}
      </div>
    `;
  }
}

window.customElements.define('language-list-item', LanguageListItem);

declare global {
  interface HTMLElementTagNameMap {
    'language-list-item': LanguageListItem;
  }
}
