// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mwc/@material/web/iconbutton/filled-icon-button.js';
import '../components/mic-selection-menu.js';
import '../components/recording-file-list.js';
import '../components/secondary-button.js';
import '../components/settings-menu.js';
import '../components/cra/cra-icon.js';
import '../components/cra/cra-icon-button.js';

import {css, html} from 'chrome://resources/mwc/lit/index.js';

import {MicSelectionMenu} from '../components/mic-selection-menu.js';
import {SettingsMenu} from '../components/settings-menu.js';
import {useRecordingDataManager} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {navigateTo} from '../core/state/route.js';
import {settings} from '../core/state/settings.js';

/**
 * Main page of Recorder App.
 */
export class MainPage extends ReactiveLitElement {
  static override styles = css`
    :host {
      background-color: var(--cros-sys-header);
      display: block;
      height: 100%;
      position: relative;
      width: 100%;
    }

    recording-file-list {
      inset: 0;
      margin: 0 16px 106px;
      position: absolute;
    }

    #actions {
      align-items: center;
      background-color: var(--cros-sys-app_base);
      border-radius: var(--border-radius-rounded-with-short-side);
      display: flex;
      flex-flow: row;
      gap: 24px;
      height: fit-content;
      inset: 0;
      margin: auto auto 32px;
      padding: 24px 44px;
      position: absolute;
      width: fit-content;
    }

    #record-button {
      --cra-icon-button-container-color: var(--cros-sys-error_container);
      --cra-icon-button-container-height: 96px;
      --cra-icon-button-container-width: 152px;
      --cros-icon-button-color-override: var(--cros-sys-on_error_container);
      --cros-icon-button-icon-size: 32px;

      @container style(--small-viewport: 1) {
        --cra-icon-button-container-height: 80px;
        --cra-icon-button-container-width: 136px;
      }

      margin: 0;
    }
  `;

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly recordingMetadataMap =
      this.recordingDataManager.getAllMetadata();

  private get micSelectionMenu(): MicSelectionMenu|null {
    return this.shadowRoot?.querySelector('mic-selection-menu') ?? null;
  }

  private get settingsMenu(): SettingsMenu|null {
    return this.shadowRoot?.querySelector('settings-menu') ?? null;
  }

  private onRecordingClick(id: string) {
    navigateTo(`/playback?id=${id}`);
  }

  private onDeleteRecordingClick(id: string) {
    // TODO(pihsun): Custom prompt instead of using `confirm()`.
    if (confirm('Really delete recording?')) {
      this.recordingDataManager.remove(id);
    }
  }

  private renderRecordButton() {
    function onClick() {
      // TODO(shik): Should we let the record page read the store value
      // directly?
      navigateTo(`/record?audioSource=${settings.value.audioSource}`);
    }
    return html`<cra-icon-button
      id="record-button"
      shape="circle"
      @click=${onClick}
    >
      <cra-icon slot="icon" name="circle_fill"></cra-icon>
    </cra-icon-button>`;
  }

  private renderMicSelectionButton() {
    const onClick = () => {
      this.micSelectionMenu?.show();
    };
    // TODO: b/336963138 - This should be a new icon-dropdown component that
    // combines button with a dropdown.
    return html`<secondary-button @click=${onClick}>
      <cra-icon slot="icon" name="mic"></cra-icon>
    </secondary-button>`;
  }

  private renderSettingsButton() {
    const onClick = () => {
      this.settingsMenu?.show();
    };
    return html`<secondary-button @click=${onClick}>
      <cra-icon slot="icon" name="settings"></cra-icon>
    </secondary-button>`;
  }

  override render(): RenderResult {
    return html`
      <recording-file-list
        .recordingMetadataMap=${this.recordingMetadataMap.value}
        @recording-clicked=${(ev: CustomEvent<string>) => {
      this.onRecordingClick(ev.detail);
    }}
        @delete-recording-clicked=${(ev: CustomEvent<string>) => {
      // TODO(pihsun): Better handling for async recording deletion.
      this.onDeleteRecordingClick(ev.detail);
    }}
      >
      </recording-file-list>
      <div id="actions">
        ${this.renderMicSelectionButton()}${this.renderRecordButton()}
        ${this.renderSettingsButton()}
      </div>
      <mic-selection-menu></mic-selection-menu>
      <settings-menu></settings-menu>
    `;
  }
}

window.customElements.define('main-page', MainPage);

declare global {
  interface HTMLElementTagNameMap {
    'main-page': MainPage;
  }
}
