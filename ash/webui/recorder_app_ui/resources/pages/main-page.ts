// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mwc/@material/web/iconbutton/filled-icon-button.js';
import '../components/cra/cra-icon-button.js';
import '../components/cra/cra-icon-dropdown.js';
import '../components/cra/cra-icon.js';
import '../components/delete-recording-dialog.js';
import '../components/educational-nudge.js';
import '../components/mic-selection-button.js';
import '../components/onboarding-dialog.js';
import '../components/recording-file-list.js';
import '../components/recording-info-dialog.js';
import '../components/secondary-button.js';
import '../components/settings-menu.js';
import '../components/system-audio-consent-dialog.js';
import '../components/error-dialog.js';
import '../components/directives/with-tooltip.js';

import {
  createRef,
  css,
  html,
  nothing,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {CraIconButton} from '../components/cra/cra-icon-button.js';
import {DeleteRecordingDialog} from '../components/delete-recording-dialog.js';
import {withTooltip} from '../components/directives/with-tooltip.js';
import {ExportDialog} from '../components/export-dialog.js';
import {OnboardingDialog} from '../components/onboarding-dialog.js';
import {RecordingFileList} from '../components/recording-file-list.js';
import {RecordingInfoDialog} from '../components/recording-info-dialog.js';
import {SettingsMenu} from '../components/settings-menu.js';
import {
  SystemAudioConsentDialog,
} from '../components/system-audio-consent-dialog.js';
import {AudioPlayerController} from '../core/audio_player_controller.js';
import {focusToBody} from '../core/focus.js';
import {i18n} from '../core/i18n.js';
import {
  useMicrophoneManager,
  useRecordingDataManager,
} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {computed, signal} from '../core/reactive/signal.js';
import {navigateTo} from '../core/state/route.js';
import {settings} from '../core/state/settings.js';
import {assertExists} from '../core/utils/assert.js';
import {isObjectEmpty} from '../core/utils/utils.js';

import {setInitialAudio} from './playback-page.js';

/**
 * Main page of Recorder App.
 */
export class MainPage extends ReactiveLitElement {
  static override styles = css`
    :host {
      --actions-padding-horizontal: 44px;
      --actions-padding-vertical: 24px;
      --record-button-height: 96px;

      @container style(--small-viewport: 1) {
        --actions-padding-horizontal: 36px;
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

    mic-selection-button {
      anchor-name: --mic-selection-button;
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
      gap: 28px;
      height: fit-content;
      inset: 0;
      margin: auto auto 32px;
      padding:
        var(--actions-padding-vertical)
        var(--actions-padding-horizontal);
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

    #choose-mic-nudge {
      flex-flow: row;
      position: fixed;
      position-anchor: --mic-selection-button;
      position-area: left;

      @container style(--small-viewport: 1) {
        flex-flow: column-reverse;
        position-area: bottom;
      }
    }

    #start-record-nudge {
      flex-flow: column;
      position: absolute;
      position-anchor: --record-button;
      position-area: top span-all;
    }
  `;

  private readonly microphoneManager = useMicrophoneManager();

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly recordingMetadataMap =
    this.recordingDataManager.getAllMetadata();

  private readonly deleteRecordingDialog = createRef<DeleteRecordingDialog>();

  private readonly systemAudioConsentDialog =
    createRef<SystemAudioConsentDialog>();

  private readonly exportDialog = createRef<ExportDialog>();

  private readonly startRecordingButton = createRef<CraIconButton>();

  private readonly recordingFileList = createRef<RecordingFileList>();

  private readonly currentPlayingId = signal<string|null>(null);

  private readonly hasOpenedMicMenu = computed(
    () => settings.value.hasOpenedMicMenu,
  );

  private readonly actionsContainerRef = createRef<HTMLElement>();

  private readonly onboardingDialogRef = createRef<OnboardingDialog>();

  private readonly currentPlayingRecordingLength = computed(() => {
    const id = this.currentPlayingId.value;
    if (id === null) {
      return null;
    }
    const durationMs =
      this.recordingDataManager.getMetadata(id).value?.durationMs ?? null;
    return durationMs === null ? null : durationMs / 1000;
  });

  private readonly audioPlayer = new AudioPlayerController(
    this,
    this.currentPlayingId,
    /* autoPlay= */ true,
  );

  private readonly inlinePlayingItemInfo = computed(() => {
    if (this.currentPlayingId.value === null ||
        this.currentPlayingRecordingLength.value === null) {
      return null;
    }
    const progress = 100 *
      (this.audioPlayer.currentTime.value /
       this.currentPlayingRecordingLength.value);
    return {
      id: this.currentPlayingId.value,
      playing: this.audioPlayer.playing.value,
      progress,
    };
  });

  private lastDeleteId: string|null = null;

  // Whether there's a mic-connection-error that is not consented.
  private readonly micConnectionErrorOccurred = signal(false);

  private get settingsMenu(): SettingsMenu|null {
    return this.shadowRoot?.querySelector('settings-menu') ?? null;
  }

  get startRecordingButtonForTest(): CraIconButton {
    return assertExists(this.startRecordingButton.value);
  }

  get recordingFileListForTest(): RecordingFileList {
    return assertExists(this.recordingFileList.value);
  }

  get actionsContainer(): HTMLElement {
    return assertExists(this.actionsContainerRef.value);
  }

  private readonly recordingInfoDialog = createRef<RecordingInfoDialog>();

  private onRecordingClick(ev: CustomEvent<string>) {
    const id = ev.detail;
    const audio = this.audioPlayer.takeInnerAudio();
    if (audio.recordingId === id) {
      setInitialAudio(audio);
    } else {
      audio.revoke();
    }
    navigateTo('playback', {id: ev.detail});
  }

  private onDeleteRecordingClick(ev: CustomEvent<string>) {
    const recordingId = ev.detail;
    this.lastDeleteId = recordingId;
    // The dialog will focus back to last focused item when closed.
    this.recordingFileList.value?.focusOnOptionOfRecordingId(recordingId);
    // TODO(pihsun): Separate delete recording dialog while recording and while
    // playback/on main page, so the latter can take a recordingId and does
    // deletion inside the component.
    this.deleteRecordingDialog.value?.show();
  }

  private onDeleteRecording() {
    if (this.lastDeleteId !== null) {
      if (this.lastDeleteId === this.currentPlayingId.value) {
        this.currentPlayingId.value = null;
      }
      this.recordingDataManager.remove(this.lastDeleteId);
      this.lastDeleteId = null;
    }
  }

  private onExportRecordingClick(ev: CustomEvent<string>) {
    const dialog = assertExists(this.exportDialog.value);
    const recordingId = ev.detail;
    dialog.recordingId = recordingId;
    // The dialog will focus back to last focused item when closed.
    this.recordingFileList.value?.focusOnOptionOfRecordingId(recordingId);
    dialog.show();
  }

  private onShowRecordingInfoClick(ev: CustomEvent<string>) {
    const dialog = assertExists(this.recordingInfoDialog.value);
    const recordingId = ev.detail;
    dialog.recordingId = recordingId;
    // The dialog will focus back to last focused item when closed.
    this.recordingFileList.value?.focusOnOptionOfRecordingId(recordingId);
    dialog.show();
  }

  private onPlayRecordingClick(ev: CustomEvent<string>) {
    const recordingId = ev.detail;
    if (this.currentPlayingId.value === recordingId) {
      this.audioPlayer.togglePlaying();
    } else {
      this.currentPlayingId.value = recordingId;
    }
  }

  private onClickRecordButton() {
    // TODO(shik): Should we let the record page read the store value
    // directly?
    // TODO(pihsun): The typed route accepts string only as parameters for now.
    // Have some way to integrate with schema.ts so this is not needed?
    const includeSystemAudio = settings.value.includeSystemAudio.toString();
    const micId = this.microphoneManager.getSelectedMicId().value;
    if (micId === null) {
      this.micConnectionErrorOccurred.value = true;
      return;
    }
    navigateTo('record', {
      includeSystemAudio,
      micId,
    });
  }

  private renderStartRecordNudge() {
    if (!isObjectEmpty(this.recordingMetadataMap.value)) {
      return nothing;
    }
    return html`
      <educational-nudge id="start-record-nudge">
        ${i18n.mainStartRecordNudge}
      </educational-nudge>
    `;
  }

  private renderChooseMicNudge() {
    if (this.hasOpenedMicMenu.value) {
      return nothing;
    }
    return html`
      <educational-nudge id="choose-mic-nudge">
        ${i18n.mainChooseMicNudge}
      </educational-nudge>
    `;
  }

  private renderMicSelectionButton() {
    function onClick() {
      settings.mutate((s) => {
        s.hasOpenedMicMenu = true;
      });
    }

    return [
      this.renderChooseMicNudge(),
      html`<mic-selection-button
        @click=${onClick}
        @trigger-system-audio-consent=${this.showSystemAudioConsentDialog}
      ></mic-selection-button>`,
    ];
  }

  private renderRecordButton() {
    return [
      this.renderStartRecordNudge(),
      html`<cra-icon-button
        id="record-button"
        shape="circle"
        @click=${this.onClickRecordButton}
        ${ref(this.startRecordingButton)}
        aria-label=${i18n.mainStartRecordButtonTooltip}
        ${withTooltip()}
      >
        <cra-icon slot="icon" name="circle_fill"></cra-icon>
      </cra-icon-button>`,
    ];
  }

  private renderSettingsButton() {
    const onClick = () => {
      this.settingsMenu?.show();
    };
    return html`<secondary-button
      id="settings-header"
      @click=${onClick}
      aria-label=${i18n.settingsHeader}
      ${withTooltip()}
    >
      <cra-icon slot="icon" name="settings"></cra-icon>
    </secondary-button>`;
  }

  private onOnboardingDone() {
    settings.mutate((s) => {
      s.onboardingDone = true;
    });
    const onboardingDialog = assertExists(this.onboardingDialogRef.value);
    // Focus back to body after the dialog is closed.
    onboardingDialog.updateComplete.then(() => {
      focusToBody();
    });
  }

  private showSystemAudioConsentDialog() {
    const dialog = assertExists(this.systemAudioConsentDialog.value);
    dialog.show();
  }

  private onSystemAudioConsentDone() {
    settings.mutate((s) => {
      s.systemAudioConsentDone = true;
      s.includeSystemAudio = true;
    });
  }

  override render(): RenderResult {
    const onboarding = settings.value.onboardingDone !== true;
    const micConnectionError = this.micConnectionErrorOccurred.value;
    const inertRoot = onboarding || micConnectionError;

    return html`
      <onboarding-dialog
        ?open=${onboarding}
        @close=${this.onOnboardingDone}
        ${ref(this.onboardingDialogRef)}
      ></onboarding-dialog>
      <system-audio-consent-dialog
        ${ref(this.systemAudioConsentDialog)}
        @system-audio-consent-clicked=${this.onSystemAudioConsentDone}
      >
      </system-audio-consent-dialog>
      <delete-recording-dialog
        ${ref(this.deleteRecordingDialog)}
        @delete=${this.onDeleteRecording}
      >
      </delete-recording-dialog>
      <export-dialog ${ref(this.exportDialog)}></export-dialog>
      <recording-info-dialog ${ref(this.recordingInfoDialog)}>
      </recording-info-dialog>
      <error-dialog
        header=${i18n.micConnectionErrorDialogHeader}
        ?open=${micConnectionError}
        @close=${() => this.micConnectionErrorOccurred.value = false}
      >
        ${i18n.micConnectionErrorDialogDescription}
      </error-dialog>
      <div id="root" ?inert=${inertRoot}>
        <recording-file-list
          .recordingMetadataMap=${this.recordingMetadataMap.value}
          .inlinePlayingItem=${this.inlinePlayingItemInfo.value}
          @recording-clicked=${this.onRecordingClick}
          @delete-recording-clicked=${this.onDeleteRecordingClick}
          @export-recording-clicked=${this.onExportRecordingClick}
          @show-recording-info-clicked=${this.onShowRecordingInfoClick}
          @play-recording-clicked=${this.onPlayRecordingClick}
          ${ref(this.recordingFileList)}
        >
        </recording-file-list>
        <div
          id="actions"
          aria-label=${i18n.mainRecordingBarLandmarkAriaLabel}
          role="region"
          part="actions"
          ${ref(this.actionsContainerRef)}
        >
          ${this.renderMicSelectionButton()}
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
