// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../components/audio-waveform.js';
import '../components/cra/cra-button.js';
import '../components/cra/cra-dialog.js';
import '../components/cra/cra-icon-button.js';
import '../components/cra/cra-image.js';
import '../components/cra/cra-menu.js';
import '../components/cra/cra-menu-item.js';
import '../components/delete-recording-dialog.js';
import '../components/recording-file-list.js';
import '../components/secondary-button.js';
import '../components/transcription-view.js';
import '../components/transcription-consent-dialog.js';

import {
  classMap,
  createRef,
  css,
  html,
  live,
  nothing,
  PropertyDeclarations,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {CraButton} from '../components/cra/cra-button.js';
import {CraDialog} from '../components/cra/cra-dialog.js';
import {CraMenu} from '../components/cra/cra-menu.js';
import {DeleteRecordingDialog} from '../components/delete-recording-dialog.js';
import {
  TranscriptionConsentDialog,
} from '../components/transcription-consent-dialog.js';
import {i18n, replacePlaceholderWithHtml} from '../core/i18n.js';
import {
  useMicrophoneManager,
  usePlatformHandler,
  useRecordingDataManager,
} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {computed, Dispose, effect, signal} from '../core/reactive/signal.js';
import {RecordingCreateParams} from '../core/recording_data_manager.js';
import {RecordingSession} from '../core/recording_session.js';
import {navigateTo} from '../core/state/route.js';
import {
  settings,
  SpeakerLabelEnableState,
  TranscriptionEnableState,
  TranscriptionLanguage,
} from '../core/state/settings.js';
import {
  assertExhaustive,
  assertExists,
  assertInstanceof,
} from '../core/utils/assert.js';
import {AsyncJobQueue} from '../core/utils/async_job_queue.js';
import {formatDuration} from '../core/utils/datetime.js';

function getDefaultTitle(): string {
  // The default title is always in English and not translated, since it's also
  // used as exported filename.
  const now = new Date();
  const year = now.getFullYear();
  const month = (now.getMonth() + 1).toString().padStart(2, '0');
  const day = now.getDate().toString().padStart(2, '0');
  const time = new Intl.DateTimeFormat('en-US', {
    hour: 'numeric',
    minute: '2-digit',
    second: '2-digit',
    hour12: true,
  });
  return `Audio recording ${year}-${month}-${day} ${time.format(now)}`;
}

/**
 * Record page of Recorder App.
 */
export class RecordPage extends ReactiveLitElement {
  static override styles = css`
    :host {
      background-color: var(--cros-sys-app_base);
      display: block;
      height: 100%;
      width: 100%;
    }

    #container {
      box-sizing: border-box;
      display: flex;
      flex-flow: column;
      gap: 4px;
      height: 100%;
      padding: 16px;
      width: 100%;
    }

    #main-area {
      border-radius: 16px;
      display: flex;
      flex: 1;
      flex-flow: column;
      gap: 4px;

      /* To have the border-radius applied to content. */
      overflow: hidden;
    }

    .sheet {
      background-color: var(--cros-sys-app_base_shaded);
      border-radius: 4px;
    }

    #header {
      align-items: center;
      display: flex;
      flex-flow: row;
      padding: 8px;
    }

    #title {
      flex: 1;
      font: var(--cros-button-1-font);
      margin: 0 0 0 12px;
    }

    #middle {
      display: flex;
      flex: 1;
      flex-flow: row;
      gap: 4px;
    }

    #audio-waveform-container,
    #transcription-container {
      /*
       * Makes both side full height without having these "expand" the main
       * area height when it's too high.
       */
      box-sizing: border-box;
      flex: 1;
      height: 0;
      min-height: 100%;
    }

    #transcription-container {
      align-items: center;
      display: none;
      justify-content: center;
    }

    .show-transcription {
      #transcription-container {
        display: flex;
      }

      @container style(--small-viewport: 1) {
        #audio-waveform-container {
          display: none;
        }
      }
    }

    #transcription-waiting {
      font: var(--cros-display-6-font);
    }

    #transcription-consent {
      align-items: center;
      display: flex;
      flex-flow: column;
      text-align: center;
      width: 352px;

      & > .header {
        font: var(--cros-headline-1-font);
        margin-top: 16px;
      }

      & > .description {
        font: var(--cros-body-2-font);
        margin-top: 8px;

        & > cra-icon {
          display: inline-block;
          height: 20px;
          vertical-align: middle;
          width: 20px;
        }
      }

      & > .actions {
        display: flex;
        flex-flow: row;
        gap: 8px;
        margin-top: 16px;
      }
    }

    audio-waveform,
    transcription-view {
      box-sizing: border-box;
      height: 100%;
      width: 100%;
    }

    #audio-waveform-container {
      position: relative;

      & > cra-icon-button {
        bottom: 28px;
        left: 0;
        margin: 0 auto;
        position: absolute;
        right: 0;
        width: fit-content;
      }
    }

    #footer {
      align-items: center;
      display: flex;
      flex-flow: column;
      gap: 24px;
      padding: 24px 0 16px;
    }

    @keyframes pulses {
      0% {
        /*
         * This is var(--cros-ref-motion-easing-emphasized-accelerate), but
         * using the CSS variable here doesn't work for some reason...
         */
        animation-timing-function: cubic-bezier(0.3, 0, 0.8, 0.15);
      }

      50% {
        opacity: 0.5;
      }
    }

    #timer {
      align-items: center;
      display: flex;
      flex-flow: row;
      font: 440 24px/32px var(--monospace-font-family);
      gap: 8px;
      height: 32px;
      letter-spacing: 0.03em;

      & > svg {
        color: var(--cros-sys-on_error_container);
        height: 12px;
        transition:
          color 500ms var(--cros-ref-motion-easing-emphasized-accelerate),
          opacity 500ms var(--cros-ref-motion-easing-emphasized-accelerate);
        width: 12px;

        .paused & {
          animation: 1s infinite pulses;
          color: var(--cros-sys-secondary);
        }
      }

      & > span {
        vertical-align: middle;
      }
    }

    #actions {
      align-items: center;
      display: flex;
      flex-flow: row;
      gap: 24px;
    }

    #pause-button {
      transition: opacity 500ms
        var(--cros-ref-motion-easing-emphasized-accelerate);

      .paused & {
        animation: 1s infinite pulses;
      }
    }

    #stop-record {
      --cra-button-container-height: 96px;
      --cra-button-icon-gap: 10px;
      --cra-button-label-text-font-family: var(
        --cros-display-6_regular-font-family
      );
      --cra-button-label-text-line-height: var(
        --cros-display-6_regular-line-height
      );
      --cra-button-label-text-size: var(--cros-display-6_regular-font-size);
      --cra-button-label-text-weight: var(--cros-display-6_regular-font-weight);
      --cra-button-leading-space: 48px;
      --cra-button-trailing-space: 48px;
      --cra-button-hover-state-layer-color: var(--cros-sys-hover_on_subtle);
      --cra-button-pressed-state-layer-color: var(
        --cros-sys-ripple_neutral_on_subtle
      );
      --cros-button-max-width: 400px;

      /*
       * TODO: b/336963138 - Currently the ripple still use the primary color,
       * which looks very bad.
       */
      --md-filled-button-container-color: var(--cros-sys-error_container);
      --md-filled-button-label-text-color: var(--cros-sys-on_error_container);
      --md-filled-button-hover-label-text-color: var(
        --cros-sys-on_error_container
      );
      --md-filled-button-pressed-label-text-color: var(
        --cros-sys-on_error_container
      );
      --md-filled-button-focus-label-text-color: var(
        --cros-sys-on_error_container
      );

      margin: 0;

      @container style(--small-viewport: 1) {
        --cra-button-container-height: 80px;
        --cra-button-leading-space: 32px;
        --cra-button-trailing-space: 32px;
      }

      & > cra-icon {
        height: 32px;
        width: 32px;
      }
    }

    #exit-dialog {
      width: 440px;

      & div[slot="actions"] cra-button:first-child {
        align-self: flex-start;
        margin-right: auto;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    includeSystemAudio: {type: Boolean},
    micId: {type: String},
  };

  includeSystemAudio = false;

  micId: string|null = null;

  private readonly recordingTitle: string = getDefaultTitle();

  private readonly recordingSession = signal<RecordingSession|null>(null);

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly platformHandler = usePlatformHandler();

  private readonly microphoneManager = useMicrophoneManager();

  // TODO: b/336963138 - Handle when transcription isn't available.
  private readonly transcriptionShown = signal(false);

  private readonly transcriptionEnabled = computed(
    () =>
      settings.value.transcriptionEnabled === TranscriptionEnableState.ENABLED,
  );

  private readonly transcriptionAvailable = computed(
    () => this.platformHandler.sodaState.value.kind !== 'unavailable',
  );

  private transcriptionEnableDispose: Dispose|null = null;

  private readonly menu = createRef<CraMenu>();

  private readonly deleteDialog = createRef<DeleteRecordingDialog>();

  private readonly transcriptionConsentDialog =
    createRef<TranscriptionConsentDialog>();

  private readonly stopRecordingButton = createRef<CraButton>();

  private wakeLock: WakeLockSentinel|null = null;

  private readonly wakeLockRequestQueue = new AsyncJobQueue('keepLatest');

  private readonly micMuted = signal(false);

  private readonly recordingPaused = signal(false);

  private readonly recordingControlQueue = new AsyncJobQueue('enqueue');

  private recordStartTime: number|null = null;

  get stopRecordingButtonForTest(): CraButton {
    return assertExists(this.stopRecordingButton.value);
  }

  private async startRecording() {
    if (this.recordingSession.value !== null) {
      return;
    }

    const speakerLabelEnabled = this.platformHandler.canUseSpeakerLabel.value &&
      settings.value.speakerLabelEnabled === SpeakerLabelEnableState.ENABLED;

    const session = await RecordingSession.create({
      micId: assertExists(this.micId),
      includeSystemAudio: this.includeSystemAudio,
      platformHandler: this.platformHandler,
      speakerLabelEnabled,
      canCaptureSystemAudioWithLoopback:
        this.platformHandler.canCaptureSystemAudioWithLoopback.value,
    });

    try {
      // Don't enable SODA if it's unavailable. All UI to enable transcription
      // are gated behind transcriptionAvailable.
      await session.start(
        this.transcriptionEnabled.value && this.transcriptionAvailable.value,
      );
    } catch (e) {
      if (e instanceof DOMException &&
          e.message.includes('Permission denied')) {
        // Permission denied, maybe user clicked cancel. Return to the main
        // page in this case.
        // TODO(pihsun): Better error handling/reporting and ask user to retry.
        navigateTo('main');
      } else {
        console.error(e);
      }
      return;
    }
    this.recordStartTime = performance.now();

    this.transcriptionEnableDispose = effect(() => {
      // TODO(pihsun): This is a bit fragile now since this relies on the
      // startNewSodaSession and stopSodaSession both calls AsyncJobQueue,
      // which always run things async so signal won't be tracked as
      // dependency. Since we only want the transcriptionEnabled as dependency
      // here, add either watch() to manually specify dependencies for effect,
      // or add untrack() to specify region that dependencies shouldn't be
      // tracked.
      if (this.transcriptionEnabled.value &&
          this.transcriptionAvailable.value) {
        session.startNewSodaSession();
      } else {
        session.stopSodaSession();
      }
    });
    this.recordingSession.value = session;
    if (settings.value.keepScreenOn) {
      this.requestWakeLock();
    }
  }

  private requestWakeLock() {
    this.wakeLockRequestQueue.push(async () => {
      // Don't request a new wake lock when the old one is still in effect,
      // since according to
      // https://w3c.github.io/screen-wake-lock/#garbage-collection we
      // shouldn't drop the wake lock that is not released.
      if (this.wakeLock === null || this.wakeLock.released) {
        this.wakeLock = await navigator.wakeLock.request('screen');
      }
    });
  }

  private releaseWakeLock() {
    this.wakeLockRequestQueue.push(async () => {
      if (this.wakeLock !== null) {
        await this.wakeLock.release();
        this.wakeLock = null;
      }
    });
  }

  private usingInternalMicrophone(): boolean {
    const microphones = this.microphoneManager.getMicrophoneList().value;
    const info = microphones.find((device) => device.deviceId === this.micId);
    return info?.isInternal ?? false;
  }

  private sendRecordEvent(recordingSaved: boolean) {
    const session = this.recordingSession.value;
    if (session === null || this.recordStartTime === null) {
      return;
    }

    const transcription = session.progress.value.transcription;
    const locale = this.transcriptionEnabled.value ?
      TranscriptionLanguage.EN_US :
      TranscriptionLanguage.NONE;

    this.platformHandler.eventsSender.sendRecordEvent({
      audioDuration: Math.round(session.progress.value.length * 1000),
      everMuted: session.everMuted,
      everPaused: session.everPaused,
      includeSystemAudio: this.includeSystemAudio,
      isInternalMicrophone: this.usingInternalMicrophone(),
      recordDuration: Math.round(performance.now() - this.recordStartTime),
      recordingSaved,
      speakerCount: transcription?.getSpeakerLabels().length ?? 0,
      speakerLabelEnableState: settings.value.speakerLabelEnabled,
      transcriptionAvailable: this.transcriptionAvailable.value,
      transcriptionEnableState: settings.value.transcriptionEnabled,
      transcriptionLocale: locale,
      wordCount: transcription?.wordCount ?? 0,
    });
  }

  private async cancelRecording() {
    if (this.recordingSession.value === null) {
      return;
    }
    this.releaseWakeLock();
    await this.recordingSession.value.finish();
    this.sendRecordEvent(/* recordingSaved= */ false);
    this.transcriptionEnableDispose?.();
    this.transcriptionEnableDispose = null;
    this.recordingSession.value = null;
  }

  /**
   * Stops and saves the recording.
   *
   * @return The id of the saved recording.
   */
  private async stopRecording(): Promise<string|null> {
    if (this.recordingSession.value === null) {
      return null;
    }
    this.releaseWakeLock();
    const session = this.recordingSession.value;

    this.platformHandler.perfLogger.start({
      audioDuration: Math.round(session.progress.value.length * 1000),
      kind: 'record',
      wordCount: session.progress.value.transcription?.wordCount ?? 0,
    });

    const audioData = await session.finish();
    const params: RecordingCreateParams = {
      title: this.recordingTitle,
      durationMs: Math.round(session.progress.value.length * 1000),
      recordedAt: Date.now(),
      powers: session.progress.value.powers.array,
      transcription: session.progress.value.transcription,
    };
    const id = await this.recordingDataManager.createRecording(
      params,
      audioData,
    );
    this.sendRecordEvent(/* recordingSaved= */ true);
    this.platformHandler.perfLogger.finish('record');

    this.transcriptionEnableDispose?.();
    this.transcriptionEnableDispose = null;
    this.recordingSession.value = null;
    return id;
  }

  private onStopRecording() {
    this.recordingControlQueue.push(async () => {
      const id = await this.stopRecording();
      if (id !== null) {
        navigateTo('playback', {id});
      }
    });
  }

  private readonly onVisibilityChange = () => {
    if (this.wakeLock !== null && this.wakeLock.released &&
        document.visibilityState === 'visible') {
      // Re-acquire the wake lock if the recorder app is brought back to
      // foreground.
      // See https://developer.chrome.com/docs/capabilities/web-apis/wake-lock/
      // TODO(pihsun): We need to have a private API if it's required to have
      // screen kept on when recording in background.
      this.requestWakeLock();
    }
  };

  override connectedCallback(): void {
    super.connectedCallback();
    // TODO(pihsun): auto-starting the recording since this page is arrived
    // from clicking "record" button from the main page. Reconsider how to do
    // this properly.
    // TODO(pihsun): Check if ignoring this promise return would be fine.
    void this.startRecording();
    document.addEventListener('visibilitychange', this.onVisibilityChange);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    document.removeEventListener('visibilitychange', this.onVisibilityChange);
    // Cancel current recording when leaving page / hot reloading.
    // TODO: b/336963138 - Have a confirmation before leaving.
    // TODO: b/336963138 - Exit handler for the whole page.
    // TODO(pihsun): Check if ignoring this promise return would be fine.
    void this.cancelRecording();
  }

  private toggleTranscriptionShown() {
    this.transcriptionShown.update((s) => !s);
  }

  private toggleTranscriptionEnabled() {
    switch (settings.value.transcriptionEnabled) {
      case TranscriptionEnableState.ENABLED:
        settings.mutate((s) => {
          s.transcriptionEnabled = TranscriptionEnableState.DISABLED;
        });
        return;
      case TranscriptionEnableState.DISABLED:
        settings.mutate((s) => {
          s.transcriptionEnabled = TranscriptionEnableState.ENABLED;
        });
        this.platformHandler.installSoda();
        return;
      case TranscriptionEnableState.UNKNOWN:
      case TranscriptionEnableState.DISABLED_FIRST:
        this.transcriptionConsentDialog.value?.show();
        return;
      default:
        assertExhaustive(settings.value.transcriptionEnabled);
    }
  }

  private get exitDialog(): CraDialog|null {
    const el = this.shadowRoot?.querySelector('#exit-dialog') ?? null;
    if (el === null) {
      return null;
    }
    return assertInstanceof(el, CraDialog);
  }

  private onDeleteButtonClick() {
    this.deleteDialog.value?.show();
  }

  private onPauseButtonClick() {
    this.recordingControlQueue.push(async () => {
      this.recordingPaused.update((s) => !s);
      // TODO(pihsun): Animate when paused state change.
      await this.recordingSession.value?.setPaused(this.recordingPaused.value);
    });
  }

  private async deleteRecording() {
    // TODO(pihsun): Make this function sync since it's called as event handler.
    await this.cancelRecording();
    navigateTo('main');
  }

  private onToggleMuted() {
    this.micMuted.update((s) => !s);
    this.recordingSession.value?.setMicMuted(this.micMuted.value);
  }

  private renderAudioWaveform() {
    if (this.recordingSession.value === null) {
      return nothing;
    }
    const session = this.recordingSession.value;
    return html`
      <audio-waveform .values=${session.progress.value.powers}>
      </audio-waveform>
      <cra-icon-button
        buttonstyle="toggle"
        class="with-filled-style"
        shape="circle"
        @click=${this.onToggleMuted}
        .selected=${this.micMuted.value}
        aria-label=${i18n.recordMuteButtonTooltip}
      >
        <cra-icon slot="icon" name="mic"></cra-icon>
        <cra-icon slot="selectedIcon" name="mic_mute"></cra-icon>
      </cra-icon-button>
    `;
  }

  private renderTranscription() {
    if (this.recordingSession.value === null) {
      return nothing;
    }
    // TODO: b/336963138 - Animation while opening/closing the panel.
    const session = this.recordingSession.value;
    const {transcription} = session.progress.value;
    if (transcription !== null && !transcription.isEmpty()) {
      // If there are existing transcription, it is always shown even if the
      // transcription is disabled afterwards.
      return html`<transcription-view .transcription=${transcription}>
      </transcription-view>`;
    }

    // Note that the image transcript.svg is currently placeholders and don't
    // use dynamic color tokens yet.
    // TODO: b/344785475 - Change to final illustration when ready.
    switch (settings.value.transcriptionEnabled) {
      case TranscriptionEnableState.ENABLED:
        return html`<div id="transcription-waiting">
          ${i18n.transcriptionWaitingSpeechText}
        </div>`;
      case TranscriptionEnableState.DISABLED:
      case TranscriptionEnableState.DISABLED_FIRST: {
        const description = replacePlaceholderWithHtml(
          i18n.recordTranscriptionOffDescription,
          '[3dot]',
          html`<cra-icon name="more_vertical"></cra-icon>`,
        );
        return html`
          <div id="transcription-consent">
            <cra-image name="transcription_off"></cra-image>
            <div class="header">${i18n.recordTranscriptionOffHeader}</div>
            <div class="description">${description}</div>
          </div>
        `;
      }
      case TranscriptionEnableState.UNKNOWN: {
        function disableTranscription() {
          settings.mutate((s) => {
            s.transcriptionEnabled = TranscriptionEnableState.DISABLED_FIRST;
          });
        }
        return html`
          <div id="transcription-consent">
            <cra-image name="transcription_enable"></cra-image>
            <div class="header">
              ${i18n.recordTranscriptionEntryPointHeader}
            </div>
            <div class="description">
              ${i18n.recordTranscriptionEntryPointDescription}
            </div>
            <div class="actions">
              <cra-button
                .label=${i18n.recordTranscriptionEntryPointDisableButton}
                button-style="secondary"
                @click=${disableTranscription}
              ></cra-button>
              <cra-button
                .label=${i18n.recordTranscriptionEntryPointEnableButton}
                @click=${this.toggleTranscriptionEnabled}
              ></cra-button>
            </div>
          </div>
        `;
      }
      default:
        assertExhaustive(settings.value.transcriptionEnabled);
    }
  }

  private renderTimer() {
    if (this.recordingSession.value === null) {
      return nothing;
    }

    const recordingLength = formatDuration(
      {
        seconds: this.recordingSession.value.progress.value.length,
      },
      1,
    );
    return html`<svg viewbox="0 0 12 12">
        <circle cx="6" cy="6" r="6" fill="currentColor" />
      </svg>
      <span>${recordingLength}</span>`;
  }

  private renderStopRecordButton() {
    return html`<cra-button
      id="stop-record"
      shape="circle"
      .label=${i18n.recordStopButton}
      @click=${this.onStopRecording}
      ${ref(this.stopRecordingButton)}
    >
      <cra-icon slot="leading-icon" name="stop"></cra-icon>
    </cra-button>`;
  }

  private async saveAndExitRecording() {
    await this.stopRecording();
    navigateTo('main');
  }

  private onSaveClick() {
    this.recordingControlQueue.push(async () => {
      await this.saveAndExitRecording();
    });
  }

  private onBackClick() {
    this.recordingControlQueue.push(async () => {
      if (this.recordingPaused.value) {
        await this.saveAndExitRecording();
      } else {
        this.exitDialog?.show();
      }
    });
  }

  private closeExitDialog() {
    this.exitDialog?.close();
  }

  private renderExitRecordingDialog() {
    return html`<cra-dialog id="exit-dialog">
      <div slot="headline">${i18n.recordExitDialogHeader}</div>
      <div slot="content">${i18n.recordExitDialogDescription}</div>
      <div slot="actions">
        <cra-button
          .label=${i18n.recordExitDialogDeleteButton}
          button-style="secondary"
          @click=${this.deleteRecording}
        ></cra-button>
        <cra-button
          .label=${i18n.recordExitDialogCancelButton}
          button-style="secondary"
          @click=${this.closeExitDialog}
        ></cra-button>
        <cra-button
          .label=${i18n.recordExitDialogSaveAndExitButton}
          @click=${this.onSaveClick}
        ></cra-button>
      </div>
    </cra-dialog>`;
  }

  private renderMenu() {
    const transcriptionMenuItem = html`
      <cra-menu-item
        headline=${i18n.recordMenuToggleTranscriptionOption}
        itemEnd="switch"
        .switchSelected=${live(this.transcriptionEnabled.value)}
        @cros-menu-item-triggered=${this.toggleTranscriptionEnabled}
      >
      </cra-menu-item>
    `;
    return html`
      <cra-menu ${ref(this.menu)} anchor="show-menu">
        <cra-menu-item
          headline=${i18n.recordMenuDeleteOption}
          @cros-menu-item-triggered=${this.onDeleteButtonClick}
        >
        </cra-menu-item>
        ${this.transcriptionAvailable.value ? transcriptionMenuItem : nothing}
      </cra-menu>
    `;
  }

  private toggleMenu() {
    this.menu.value?.toggle();
  }

  private renderHeader() {
    const toggleTranscriptionButton = html`
      <cra-icon-button
        buttonstyle="toggle"
        @click=${this.toggleTranscriptionShown}
        aria-expanded=${this.transcriptionShown.value}
        aria-label=${i18n.recordTranscriptButtonTooltip}
      >
        <cra-icon slot="icon" name="notes"></cra-icon>
        <cra-icon slot="selectedIcon" name="notes"></cra-icon>
      </cra-icon-button>
    `;
    return html`
      <div id="header" class="sheet">
        <cra-icon-button
          buttonstyle="floating"
          @click=${this.onBackClick}
          aria-label=${i18n.backToMainButtonAriaLabel}
        >
          <cra-icon slot="icon" name="arrow_back"></cra-icon>
        </cra-icon-button>
        <span id="title">${this.recordingTitle}</span>
        ${
      this.transcriptionAvailable.value ? toggleTranscriptionButton : nothing}
        <cra-icon-button
          buttonstyle="floating"
          @click=${this.toggleMenu}
          id="show-menu"
          aria-label=${i18n.recordMenuButtonTooltip}
        >
          <cra-icon slot="icon" name="more_vertical"></cra-icon>
        </cra-icon-button>
      </div>
      ${this.renderMenu()}
    `;
  }

  override render(): RenderResult {
    const mainSectionClasses = {
      'show-transcription': this.transcriptionShown.value,
    };

    const footerClasses = {
      paused: this.recordingPaused.value,
    };

    return html`
      <div id="container" part="container">
        <div id="main-area">
          ${this.renderHeader()}
          <div id="middle" class=${classMap(mainSectionClasses)}>
            <div id="audio-waveform-container" class="sheet">
              ${this.renderAudioWaveform()}
            </div>
            <div id="transcription-container" class="sheet">
              ${this.renderTranscription()}
            </div>
          </div>
        </div>
        <div id="footer" class=${classMap(footerClasses)}>
          <div id="timer">${this.renderTimer()}</div>
          <div id="actions">
            <secondary-button
              @click=${this.onDeleteButtonClick}
              aria-label=${i18n.recordDeleteButtonTooltip}
            >
              <cra-icon slot="icon" name="delete"></cra-icon>
            </secondary-button>
            ${this.renderStopRecordButton()}
            <secondary-button
              id="pause-button"
              @click=${this.onPauseButtonClick}
              aria-label=${i18n.recordPauseButtonTooltip}
              buttonstyle="toggle"
              class="with-filled-style"
              .selected=${this.recordingPaused.value}
            >
              <cra-icon slot="icon" name="pause"></cra-icon>
              <cra-icon slot="selectedIcon" name="pause"></cra-icon>
            </secondary-button>
          </div>
        </div>
      </div>
      <delete-recording-dialog
        current
        @delete=${this.deleteRecording}
        ${ref(this.deleteDialog)}
      >
      </delete-recording-dialog>
      ${this.renderExitRecordingDialog()}
      <transcription-consent-dialog ${ref(this.transcriptionConsentDialog)}>
      </transcription-consent-dialog>
    `;
  }
}

window.customElements.define('record-page', RecordPage);

declare global {
  interface HTMLElementTagNameMap {
    'record-page': RecordPage;
  }
}
