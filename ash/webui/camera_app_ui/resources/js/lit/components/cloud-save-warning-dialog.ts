// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createRef, css, html, LitElement, nothing, ref} from 'chrome://resources/mwc/lit/index.js';

import {assertExists} from '../../assert.js';
import {I18nString} from '../../i18n_string.js';
import {getCloudDestination, getI18nMessage, isCloudDestination, isCloudDestinationOnedrive} from '../../models/load_time_data.js';
import * as localStorage from '../../models/local_storage.js';
import {LocalStorageKey} from '../../type.js';
import {DEFAULT_STYLE} from '../styles.js';

export class CloudSaveWarningDialog extends LitElement {
  static override styles = [
    DEFAULT_STYLE,
    css`
      /* Native <dialog> styled to match cr-dialog appearance. */
      dialog {
        background-color: var(--cros-sys-base_elevated);
        border: 0;
        border-radius: 8px;
        box-shadow:
          0 0 16px rgba(0, 0, 0, 0.12),
          0 16px 16px rgba(0, 0, 0, 0.24);
        color: var(--cros-sys-on_surface);
        max-height: initial;
        max-width: initial;
        overflow: hidden;
        padding: 0;
        width: 416px;
      }

      dialog::backdrop {
        background-color: rgba(0, 0, 0, 0.6);
      }

      #title-slot {
        font: var(--cros-title-1-font);
        padding: 20px 20px 16px;
      }

      #body-slot {
        color: var(--cros-sys-secondary);
        padding: 0 20px;
      }

      #button-container {
        display: flex;
        justify-content: flex-end;
        padding: 16px;
      }

      .space-between {
        display: flex;
        flex-direction: column;
        gap: 16px;
      }

      #enterprise-icon {
        color: var(--cros-sys-primary);
        display: inline-block;
      }

      /* matches cr-button.action-button with border-radius 20px. */
      #ack-button {
        -webkit-tap-highlight-color: transparent;
        background: var(--cros-sys-primary);
        border: none;
        border-radius: 20px;
        color: var(--cros-sys-on_primary);
        cursor: pointer;
        font: var(--cros-button-2-font);
        min-width: 5.14em;
        padding: 8px 16px;
        user-select: none;
      }

      #ack-button:hover {
        background: linear-gradient(
          var(--cros-sys-hover_on_prominent),
          var(--cros-sys-hover_on_prominent)
        ), var(--cros-sys-primary);
      }

      #ack-button:focus-visible {
        outline: 2px solid var(--cros-sys-focus_ring);
        outline-offset: 2px;
      }

      /* Checkbox label row - matches cr-checkbox layout. */
      #dont-show-label {
        -webkit-tap-highlight-color: transparent;
        align-items: center;
        cursor: pointer;
        display: flex;
        gap: 20px;
        user-select: none;
      }

      /* Styled native checkbox - matches cr-checkbox box appearance. */
      #dont-show-checkbox {
        -webkit-appearance: none;
        appearance: none;
        background: none;
        border: 2px solid var(--cros-sys-outline);
        border-radius: 2px;
        box-sizing: border-box;
        cursor: pointer;
        display: block;
        flex-shrink: 0;
        height: 16px;
        margin: 0;
        outline: none;
        padding: 0;
        width: 16px;
      }

      #dont-show-checkbox:checked {
        background-color: var(--cros-sys-primary);
        /* Material Design checkmark as SVG data URI (fill=white). */
        background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'%3E%3Cpath d='M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z' fill='white'/%3E%3C/svg%3E");
        background-position: center;
        background-repeat: no-repeat;
        background-size: 12px;
        border-color: var(--cros-sys-primary);
      }

      #dont-show-checkbox:focus-visible {
        outline: 2px solid var(--cros-sys-focus_ring);
        outline-offset: 2px;
      }
    `,
  ];

  private readonly dialog = createRef<HTMLDialogElement>();

  private readonly dontShowAgainCheckbox = createRef<HTMLInputElement>();

  close(): void {
    assertExists(this.dialog.value).close();
    localStorage.set(
        LocalStorageKey.PREF_SKIP_CLOUD_SAVE_WARNING,
        assertExists(this.dontShowAgainCheckbox.value).checked);
  }

  skipCloudSaveWarning: boolean;

  constructor() {
    super();

    const lastCloudDestination =
        localStorage.getString(LocalStorageKey.PREF_LAST_CLOUD_DESTINATION);
    const cloudDestination = getCloudDestination();
    if (lastCloudDestination !== cloudDestination) {
      // Reset the skip preference if the cloud destination has changed.
      localStorage.remove(LocalStorageKey.PREF_SKIP_CLOUD_SAVE_WARNING);
      localStorage.set(
          LocalStorageKey.PREF_LAST_CLOUD_DESTINATION, cloudDestination);
    }
    this.skipCloudSaveWarning = !isCloudDestination() ||
        localStorage.getBool(LocalStorageKey.PREF_SKIP_CLOUD_SAVE_WARNING);
  }

  override firstUpdated(): void {
    if (!this.skipCloudSaveWarning) {
      assertExists(this.dialog.value).showModal();
    }
  }

  override render(): RenderResult {
    if (this.skipCloudSaveWarning) {
      return nothing;
    }
    return html`
      <dialog ${ref(this.dialog)}>
        <div id="title-slot" class="space-between">
          <div style="text-align: left;">
            <svg-wrapper id="enterprise-icon" name="enterprise.svg">
            </svg-wrapper>
          <div class="icon" data-svg="settings_resolution.svg"></div>
          </div>
          <div>
            ${
        getI18nMessage(
            isCloudDestinationOnedrive() ?
                I18nString.CLOUD_SAVE_WARNING_DIALOG_TITLE_ONEDRIVE :
                I18nString.CLOUD_SAVE_WARNING_DIALOG_TITLE_GOOGLE_DRIVE)}
          </div>
        </div>
        <div id="body-slot" class="space-between">
          <div>${
        getI18nMessage(I18nString.CLOUD_SAVE_WARNING_DIALOG_DISCLAIMER)}
          </div>
          <div>
            <label id="dont-show-label">
              <input type="checkbox" id="dont-show-checkbox"
                     ${ref(this.dontShowAgainCheckbox)}>
              ${
        getI18nMessage(I18nString.CLOUD_SAVE_WARNING_DIALOG_SKIP_CHECKBOX)}
            </label>
          </div>
        </div>
        <div id="button-container">
          <button id="ack-button" @click=${this.close}>
            ${getI18nMessage(I18nString.CLOUD_SAVE_WARNING_DIALOG_ACK_BUTTON)}
          </button>
        </div>
      </dialog>
    `;
  }
}

window.customElements.define(
    'cloud-save-warning-dialog', CloudSaveWarningDialog);

declare global {
  interface HTMLElementTagNameMap {
    'cloud-save-warning-dialog': CloudSaveWarningDialog;
  }
}
