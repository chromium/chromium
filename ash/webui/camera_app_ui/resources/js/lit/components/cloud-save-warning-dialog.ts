// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {
  CrCheckboxElement,
} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {
  CrDialogElement,
} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {
  createRef,
  css,
  html,
  LitElement,
  nothing,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {assertExists} from '../../assert.js';
import {I18nString} from '../../i18n_string.js';
import {
  getCloudDestination,
  getI18nMessage,
  isCloudDestination,
  isCloudDestinationOnedrive,
} from '../../models/load_time_data.js';
import * as localStorage from '../../models/local_storage.js';
import {LocalStorageKey} from '../../type.js';
import {DEFAULT_STYLE} from '../styles.js';

export class CloudSaveWarningDialog extends LitElement {
  static override styles = [
    DEFAULT_STYLE,
    css`
      cr-dialog {
        --cr-dialog-width: 416px;
      }
      #enterprise-icon {
        color: var(--cros-sys-primary);
        display: inline-block;
      }
      .rounded-button {
        border-radius: 20px;
      }
      .space-between {
        display: flex;
        flex-direction: column;
        gap: 16px;
      }
    `,
  ];

  private readonly dialog = createRef<CrDialogElement>();

  private readonly dontShowAgainCheckbox = createRef<CrCheckboxElement>();

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

  override render(): RenderResult {
    if (this.skipCloudSaveWarning) {
      return nothing;
    }
    return html`
      <cr-dialog show-on-attach close-text="close" ${ref(this.dialog)}>
        <div slot="title" class="space-between">
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
        <div slot="body" class="space-between">
          <div>${
        getI18nMessage(I18nString.CLOUD_SAVE_WARNING_DIALOG_DISCLAIMER)}
          </div>
          <div>
            <cr-checkbox ${ref(this.dontShowAgainCheckbox)}>
              ${
        getI18nMessage(I18nString.CLOUD_SAVE_WARNING_DIALOG_SKIP_CHECKBOX)}
            </cr-checkbox>
          </div>
        </div>
        <div slot="button-container">
          <cr-button @click=${this.close}
                     class="action-button rounded-button">
            ${getI18nMessage(I18nString.CLOUD_SAVE_WARNING_DIALOG_ACK_BUTTON)}
          </cr-button>
        </div>
      </cr-dialog>
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
