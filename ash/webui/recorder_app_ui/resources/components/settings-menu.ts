// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/switch/switch.js';
import './cra/cra-dialog.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './settings-row.js';

import {css, html} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';

import {CraDialog} from './cra/cra-dialog.js';

/**
 * Settings menu for Recording app.
 */
export class SettingsMenu extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    cra-dialog {
      --md-dialog-container-color: var(--cros-sys-surface3);

      /* 16px margin for each side at minimum size. */
      max-height: calc(100% - 32px);
      max-width: calc(100% - 32px);
      width: 512px;

      @container style(--dark-theme: 1) {
        /*
         * TODO: b/336963138 - This is neutral5 in spec but there's no
         * neutral5 in colors.css.
         */
        --md-dialog-container-color: var(--cros-sys-app_base_shaded);
      }
    }

    div[slot="content"] {
      padding: 0;
    }

    #header {
      color: var(--cros-sys-primary);
      font: var(--cros-title-1-font);
      padding: 24px;
      position: relative;

      & > cra-icon-button {
        position: absolute;
        right: 16px;
        top: 16px;
      }
    }

    #body {
      background: var(--cros-sys-surface1);
      border-radius: 20px;
      display: flex;
      flex-flow: column;
      gap: 8px;
      padding: 0 16px 16px;

      @container style(--dark-theme: 1) {
        background: var(--cros-sys-app_base);
      }
    }

    .section {
      padding-top: 8px;

      & > .title {
        color: var(--cros-sys-primary);
        font: var(--cros-button-2-font);
        padding: 8px;
      }

      & > .body {
        border-radius: 16px;
        display: flex;
        flex-flow: column;
        gap: 1px;

        /* To have the border-radius applied to content. */
        overflow: hidden;
      }
    }
  `;

  private get dialog(): CraDialog|null {
    return this.shadowRoot?.querySelector('cra-dialog') ?? null;
  }

  show(): void {
    this.dialog?.show();
  }

  private onCloseClick() {
    this.dialog?.close();
  }

  override render(): RenderResult {
    // TODO: b/336963138 - Implement actual functionality of all settings.
    return html`<cra-dialog>
      <div slot="content">
        <div id="header">
          ${i18n('Recorder settings')}
          <cra-icon-button
            buttonstyle="floating"
            size="small"
            shape="circle"
            @click=${this.onCloseClick}
          >
            <cra-icon slot="icon" name="close"></cra-icon>
          </cra-icon-button>
        </div>
        <div id="body">
          <div class="section">
            <div class="title">${i18n('General')}</div>
            <div class="body">
              <settings-row>
                <span slot="label">${i18n('Do not disturb')}</span>
                <span slot="description">
                  ${i18n('Mutes notifications while recording')}
                </span>
                <cros-switch slot="action"></cros-switch>
              </settings-row>
              <settings-row>
                <span slot="label">
                  ${i18n('Keep screen on during recording')}
                </span>
                <cros-switch slot="action"></cros-switch>
              </settings-row>
            </div>
          </div>
          <div class="section">
            <div class="title">${i18n('Transcription & Google AI')}</div>
            <div class="body">
              <settings-row>
                <span slot="label">${i18n('Allow speaker ID')}</span>
                <span slot="description">
                  ${i18n('Currently only available in English (US)')}
                </span>
                <cros-switch slot="action"></cros-switch>
              </settings-row>
              <settings-row>
                <span slot="label">${i18n('Audio transcription')}</span>
                <cros-switch slot="action"></cros-switch>
              </settings-row>
              <!--
                TODO: b/336963138 - Add transcription language and summary.
              -->
            </div>
          </div>
        </div>
      </div>
    </cra-dialog>`;
  }
}

window.customElements.define('settings-menu', SettingsMenu);

declare global {
  interface HTMLElementTagNameMap {
    'settings-menu': SettingsMenu;
  }
}
