// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mwc/@material/web/focus/md-focus-ring.js';
import 'chrome://resources/mwc/@material/web/progress/circular-progress.js';
import './cra/cra-button.js';
import './cra/cra-icon.js';
import './settings-row.js';
import './spoken-message.js';

import {
  css,
  html,
  nothing,
  PropertyDeclarations,
  PropertyValues,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ModelState} from '../core/on_device_model/types.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {LangPackInfo} from '../core/soda/language_info.js';
import {
  assertExhaustive,
  assertExists,
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

  private readonly stateChanged = signal(false);

  // Announces status in the next render cycle.
  override updated(changedProperties: PropertyValues<this>): void {
    const state = changedProperties.get('sodaState');
    if (state === undefined || state.kind === 'unavailable') {
      this.stateChanged.value = false;
    } else {
      this.stateChanged.value = true;
    }
  }

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

  private renderStatusMessage(): RenderResult {
    if (!this.stateChanged.value) {
      return nothing;
    }
    const kind = this.sodaState.kind;
    const name = assertExists(this.langPackInfo).displayName;
    switch (kind) {
      case 'needsReboot':
        return html`
          <spoken-message slot="status" role="status" aria-live="polite">
            ${i18n.languagePickerLanguageNeedsRebootStatusMessage(name)}
          </spoken-message>
        `;
      case 'error':
        return html`
          <spoken-message slot="status" role="status" aria-live="polite">
            ${i18n.languagePickerLanguageDownloadErrorStatusMessage(name)}
          </spoken-message>
        `;
      case 'installing':
        return html`
          <spoken-message slot="status" role="status" aria-live="polite">
            ${i18n.languagePickerLanguageDownloadStartedStatusMessage(name)}
          </spoken-message>
        `;
      case 'installed':
        return html`
          <spoken-message slot="status" role="status" aria-live="polite">
            ${i18n.languagePickerLanguageDownloadFinishedStatusMessage(name)}
          </spoken-message>
        `;
      case 'notInstalled':
        return nothing;
      case 'unavailable':
        return assertNotReached('SODA unavailable but the row is rendered');
      default:
        return assertExhaustive(kind);
    }
  }

  private renderDescriptionAndAction(): RenderResult {
    const name = assertExists(this.langPackInfo).displayName;
    const downloadButtonAriaLabel =
      i18n.languagePickerLanguageDownloadButtonAriaLabel(name);
    const downloadButton = html`
      <cra-button
        slot="action"
        button-style="secondary"
        aria-label=${downloadButtonAriaLabel}
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
      case 'needsReboot':
        return html`
          <span slot="description" class="error">
            ${i18n.languagePickerLanguageNeedsRebootDescription}
          </span>
          ${downloadButton}
        `;
      case 'error':
        // Shows the download button for users to try again.
        return html`
          <span slot="description" class="error" aria-hidden="true">
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
          <span slot="description" aria-hidden="true">
            ${progressDescription}
          </span>
          <cra-button
            slot="action"
            aria-hidden="true"
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

  private getAriaLabel() {
    const name = assertExists(this.langPackInfo).displayName;
    const kind = this.sodaState.kind;
    switch (kind) {
      case 'notInstalled': {
        return i18n.languagePickerLanguageNotDownloadedAriaLabel(name);
      }
      case 'needsReboot': {
        return i18n.languagePickerLanguageNeedsRebootAriaLabel(name);
      }
      case 'error': {
        return i18n.languagePickerLanguageDownloadErrorAriaLabel(name);
      }
      case 'installing': {
        return i18n.languagePickerLanguageDownloadingAriaLabel(
          name,
          this.sodaState.progress,
        );
      }
      case 'installed': {
        if (this.selected) {
          return i18n.languagePickerLanguageSelectedAriaLabel(name);
        } else {
          return i18n.languagePickerLanguageNotSelectedAriaLabel(name);
        }
      }
      case 'unavailable':
        return assertNotReached('SODA unavailable but the row is rendered.');
      default:
        assertExhaustive(kind);
    }
  }

  private isFocusable() {
    // Focus on whole row only when there's no visible action button.
    if (this.sodaState.kind === 'installed' && !this.selected) {
      return true;
    }
    return false;
  }

  override render(): RenderResult {
    if (this.langPackInfo === null || this.sodaState.kind === 'unavailable') {
      return nothing;
    }
    return html`
      <div id="root"
        tabindex=${this.isFocusable() ? 0 : -1}
        role="button"
        aria-label=${this.getAriaLabel()}
        @click=${this.activateRow}
        @keydown=${this.onKeyDown}
        @keyup=${this.onKeyUp}
      >
        <settings-row>
          <span slot="label" aria-hidden="true">
            ${this.langPackInfo.displayName}
          </span>
          ${this.renderDescriptionAndAction()}
          ${this.renderStatusMessage()}
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
