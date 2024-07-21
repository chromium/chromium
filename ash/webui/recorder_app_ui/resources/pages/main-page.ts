// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mwc/@material/web/iconbutton/filled-icon-button.js';
import '../components/mic-selection-menu.js';
import '../components/onboarding-dialog.js';
import '../components/recording-file-list.js';
import '../components/secondary-button.js';
import '../components/settings-menu.js';
import '../components/cra/cra-icon.js';
import '../components/cra/cra-icon-button.js';
import '../components/delete-recording-dialog.js';

import {createRef, css, html, ref} from 'chrome://resources/mwc/lit/index.js';

import {DeleteRecordingDialog} from '../components/delete-recording-dialog.js';
import {ExportDialog} from '../components/export-dialog.js';
import {MicSelectionMenu} from '../components/mic-selection-menu.js';
import {SettingsMenu} from '../components/settings-menu.js';
import {
  useMicrophoneManager,
  useRecordingDataManager,
} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {navigateTo} from '../core/state/route.js';
import {settings} from '../core/state/settings.js';
import {assertExists, assertInstanceof} from '../core/utils/assert.js';

/**
 * Main page of Recorder App.
 */
export class MainPage extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
      height: 100%;
      width: 100%;
    }

    recording-file-list {
      inset: 0;
      margin: 0 16px 106px;
      position: absolute;
    }

    #root {
      background-color: var(--cros-sys-header);
      height: 100%;
      position: relative;
      width: 100%;
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

  private readonly microphoneManager = useMicrophoneManager();

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly recordingMetadataMap =
    this.recordingDataManager.getAllMetadata();

  private readonly deleteRecordingDialog = createRef<DeleteRecordingDialog>();

  private readonly exportDialog = createRef<ExportDialog>();

  private lastDeleteId: string|null = null;

  private get micSelectionMenu(): MicSelectionMenu|null {
    return this.shadowRoot?.querySelector('mic-selection-menu') ?? null;
  }

  private get settingsMenu(): SettingsMenu|null {
    return this.shadowRoot?.querySelector('settings-menu') ?? null;
  }

  private onRecordingClick(ev: CustomEvent<string>) {
    navigateTo(`/playback?id=${ev.detail}`);
  }

  private onDeleteRecordingClick(ev: CustomEvent<string>) {
    this.lastDeleteId = ev.detail;
    // TODO(pihsun): Separate delete recording dialog while recording and while
    // playback/on main page, so the latter can take a recordingId and does
    // deletion inside the component.
    this.deleteRecordingDialog.value?.show();
  }

  private onDeleteRecording() {
    if (this.lastDeleteId !== null) {
      this.recordingDataManager.remove(this.lastDeleteId);
      this.lastDeleteId = null;
    }
  }

  private onExportRecordingClick(ev: CustomEvent<string>) {
    const dialog = assertExists(this.exportDialog.value);
    dialog.recordingId = ev.detail;
    dialog.show();
  }

  private onClickRecordButton() {
    // TODO(shik): Should we let the record page read the store value
    // directly?
    const includeSystemAudio = settings.value.includeSystemAudio.toString();
    const micId = assertExists(
      this.microphoneManager.getSelectedMicId().value,
      'There is no selected microphone.'
    );
    navigateTo(`/record?includeSystemAudio=${includeSystemAudio}&micId=${micId}`
    );
  }

  private renderRecordButton() {
    return html`<cra-icon-button
      id="record-button"
      shape="circle"
      @click=${this.onClickRecordButton}
    >
      <cra-icon slot="icon" name="circle_fill"></cra-icon>
    </cra-icon-button>`;
  }

  private renderMicSelectionButton() {
    const onClick = (ev: Event) => {
      this.micSelectionMenu?.show(assertInstanceof(ev.target, HTMLElement));
    };
    // TODO: b/336963138 - This should be a new icon-dropdown component that
    // combines button with a dropdown.
    return html`<secondary-button @click=${onClick} id="mic-selection-button">
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
    const onboarding = settings.value.onboardingDone !== true;
    function onOnboardingDone() {
      settings.mutate((s) => {
        s.onboardingDone = true;
      });
    }
    return html`
      <onboarding-dialog
        ?open=${onboarding}
        @close=${onOnboardingDone}
      ></onboarding-dialog>
      <delete-recording-dialog
        ${ref(this.deleteRecordingDialog)}
        @delete=${this.onDeleteRecording}
      >
      </delete-recording-dialog>
      <export-dialog ${ref(this.exportDialog)}></export-dialog>
      <div id="root" ?inert=${onboarding}>
        <recording-file-list
          .recordingMetadataMap=${this.recordingMetadataMap.value}
          @recording-clicked=${this.onRecordingClick}
          @delete-recording-clicked=${this.onDeleteRecordingClick}
          @export-recording-clicked=${this.onExportRecordingClick}
        >
        </recording-file-list>
        <div id="actions">
          ${this.renderMicSelectionButton()}${this.renderRecordButton()}
          ${this.renderSettingsButton()}
        </div>
        <mic-selection-menu></mic-selection-menu>
        <settings-menu></settings-menu>
      </div>
    `;
  }
}

window.customElements.define('main-page', MainPage);

declare global {
  interface HTMLElementTagNameMap {
    'main-page': MainPage;
  }
}
