// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mwc/@material/web/iconbutton/filled-icon-button.js';
import '../components/cra/cra-icon-button.js';
import '../components/cra/cra-icon-dropdown.js';
import '../components/cra/cra-icon.js';
import '../components/delete-recording-dialog.js';
import '../components/mic-selection-button.js';
import '../components/onboarding-dialog.js';
import '../components/recording-file-list.js';
import '../components/recording-info-dialog.js';
import '../components/secondary-button.js';
import '../components/settings-menu.js';

import {
  createRef,
  css,
  html,
  nothing,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {DeleteRecordingDialog} from '../components/delete-recording-dialog.js';
import {ExportDialog} from '../components/export-dialog.js';
import {RecordingFileList} from '../components/recording-file-list.js';
import {RecordingInfoDialog} from '../components/recording-info-dialog.js';
import {SettingsMenu} from '../components/settings-menu.js';
import {i18n} from '../core/i18n.js';
import {
  useMicrophoneManager,
  useRecordingDataManager,
} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {navigateTo} from '../core/state/route.js';
import {settings} from '../core/state/settings.js';
import {assertExists} from '../core/utils/assert.js';
import {isObjectEmpty} from '../core/utils/utils.js';

/**
 * Main page of Recorder App.
 */
export class MainPage extends ReactiveLitElement {
  static override styles = css`
    :host {
      --actions-padding-vertical: 24px;
      --record-button-height: 96px;

      @container style(--small-viewport: 1) {
        --record-button-height: 80px;
      }

      display: block;
      height: 100%;
      width: 100%;
    }

    recording-file-list {
      --actions-height: calc(
        var(--actions-padding-vertical) * 2 + var(--record-button-height)
      );
      --scroll-bottom-extra-padding: calc(var(--actions-height) / 2);

      inset: 0;
      margin: 16px 16px calc(32px + var(--actions-height) / 2);
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
      padding: var(--actions-padding-vertical) 44px;
      position: absolute;
      width: fit-content;
    }

    #record-button {
      --cra-icon-button-container-color: var(--cros-sys-error_container);
      --cra-icon-button-container-height: var(--record-button-height);
      --cra-icon-button-container-width: 152px;
      --cros-icon-button-color-override: var(--cros-sys-on_error_container);
      --cros-icon-button-icon-size: 32px;

      @container style(--small-viewport: 1) {
        --cra-icon-button-container-width: 136px;
      }

      anchor-name: --record-button;
      margin: 0;
    }

    #start-record-nudge {
      align-items: center;
      display: flex;
      flex-flow: column;
      inset-area: top span-all;
      position: absolute;
      position-anchor: --record-button;

      & > div {
        background: var(--cros-sys-primary);
        border-radius: var(--border-radius-rounded-with-short-side);
        color: var(--cros-sys-on_primary);
        font: var(--cros-body-1-font);
      }

      & > .dot {
        height: 8px;
        margin: 4px;
        width: 8px;
      }

      & > .text {
        padding: 8px 16px;
      }
    }
  `;

  private readonly microphoneManager = useMicrophoneManager();

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly recordingMetadataMap =
    this.recordingDataManager.getAllMetadata();

  private readonly deleteRecordingDialog = createRef<DeleteRecordingDialog>();

  private readonly exportDialog = createRef<ExportDialog>();

  private readonly startRecordingButton = createRef<HTMLButtonElement>();

  private readonly recordingFileList = createRef<RecordingFileList>();

  private lastDeleteId: string|null = null;

  private get settingsMenu(): SettingsMenu|null {
    return this.shadowRoot?.querySelector('settings-menu') ?? null;
  }

  get startRecordingButtonForTest(): HTMLButtonElement {
    return assertExists(this.startRecordingButton.value);
  }

  get recordingFileListForTest(): RecordingFileList {
    return assertExists(this.recordingFileList.value);
  }

  private readonly recordingInfoDialog = createRef<RecordingInfoDialog>();

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

  private onShowRecordingInfoClick(ev: CustomEvent<string>) {
    const dialog = assertExists(this.recordingInfoDialog.value);
    dialog.recordingId = ev.detail;
    dialog.show();
  }

  private onClickRecordButton() {
    // TODO(shik): Should we let the record page read the store value
    // directly?
    const includeSystemAudio = settings.value.includeSystemAudio.toString();
    const micId = assertExists(
      this.microphoneManager.getSelectedMicId().value,
      'There is no selected microphone.',
    );
    navigateTo(
      `/record?includeSystemAudio=${includeSystemAudio}&micId=${micId}`,
    );
  }

  private renderStartRecordNudge() {
    if (!isObjectEmpty(this.recordingMetadataMap.value)) {
      return nothing;
    }
    return html`
      <div id="start-record-nudge">
        <div class="text">${i18n.mainStartRecordNudge}</div>
        <div class="dot"></div>
      </div>
    `;
  }

  private renderRecordButton() {
    return [
      this.renderStartRecordNudge(),
      html`<cra-icon-button
        id="record-button"
        shape="circle"
        @click=${this.onClickRecordButton}
        ${ref(this.startRecordingButton)}
      >
        <cra-icon slot="icon" name="circle_fill"></cra-icon>
      </cra-icon-button>`,
    ];
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
      <recording-info-dialog ${ref(this.recordingInfoDialog)}>
      </recording-info-dialog>
      <div id="root" ?inert=${onboarding}>
        <recording-file-list
          .recordingMetadataMap=${this.recordingMetadataMap.value}
          @recording-clicked=${this.onRecordingClick}
          @delete-recording-clicked=${this.onDeleteRecordingClick}
          @export-recording-clicked=${this.onExportRecordingClick}
          @show-recording-info-clicked=${this.onShowRecordingInfoClick}
          ${ref(this.recordingFileList)}
        >
        </recording-file-list>
        <div id="actions">
          <mic-selection-button></mic-selection-button>
          ${this.renderRecordButton()}${this.renderSettingsButton()}
        </div>
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
