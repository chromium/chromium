// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../components/audio-waveform.js';
import '../components/cra/cra-button.js';
import '../components/cra/cra-dialog.js';
import '../components/cra/cra-icon-button.js';
import '../components/recording-file-list.js';
import '../components/secondary-button.js';
import '../components/transcription-view.js';

import {
  classMap,
  css,
  html,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {CraDialog} from '../components/cra/cra-dialog.js';
import {i18n} from '../core/i18n.js';
import {
  usePlatformHandler,
  useRecordingDataManager,
} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {RecordingCreateParams} from '../core/recording_data_manager.js';
import {AudioSource, RecordingSession} from '../core/recording_session.js';
import {navigateTo} from '../core/state/route.js';
import {assertInstanceof, checkEnumVariant} from '../core/utils/assert.js';
import {formatDuration} from '../core/utils/datetime.js';

function getDefaultTitle(): string {
  const now = new Date();
  const weekday = new Intl.DateTimeFormat('en-US', {
    weekday: 'long',
  });
  const time = new Intl.DateTimeFormat('en-US', {
    hour: 'numeric',
    minute: '2-digit',
    hour12: true,
  });
  return `${weekday.format(now)} at ${time.format(now)}`;
}

/**
 * Record page of Recorder App.
 */
export class RecordPage extends ReactiveLitElement {
  static override styles = css`
    :host {
      background-color: var(--cros-sys-app_base);
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
      display: none;
    }

    .show-transcription {
      #transcription-container {
        display: block;
      }

      @container style(--small-viewport: 1) {
        #audio-waveform-container {
          display: none;
        }
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
        width: 12px;
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

    #delete-dialog {
      width: 368px;
    }

    #exit-dialog div[slot="actions"] cra-button:first-child {
      align-self: flex-start;

      /* There's a 8px gap. */
      margin-right: 72px;
    }
  `;

  static override properties: PropertyDeclarations = {
    audioSource: {type: String},
  };

  audioSource: string|null = null;

  private readonly recordingTitle: string = getDefaultTitle();

  private readonly recordingSession = signal<RecordingSession|null>(null);

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly platformHandler = usePlatformHandler();

  // TODO: b/336963138 - Handle when transcription isn't available.
  private readonly showTranscription = signal(false);

  private async startRecording() {
    if (this.recordingSession.value !== null) {
      return;
    }

    try {
      this.recordingSession.value = await RecordingSession.create({
        platformHandler: this.platformHandler,
        source: checkEnumVariant(AudioSource, this.audioSource) ??
            AudioSource.USER_MEDIA,
      });
    } catch (e) {
      if (e instanceof DOMException &&
          e.message.includes('Permission denied')) {
        // Permission denied, maybe user clicked cancel. Return to the main
        // page in this case.
        // TODO(pihsun): Better error handling/reporting and ask user to retry.
        navigateTo(`/`);
      } else {
        console.error(e);
      }
      return;
    }
    await this.recordingSession.value.start();
  }

  private async cancelRecording() {
    if (this.recordingSession.value === null) {
      return;
    }
    await this.recordingSession.value.finish();
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
    const session = this.recordingSession.value;
    const audioData = await session.finish();
    const metadata: RecordingCreateParams = {
      title: this.recordingTitle,
      durationMs: Math.round(session.progress.value.length * 1000),
      recordedAt: Date.now(),
      powers: session.progress.value.powers,
      textTokens: session.progress.value.textTokens,
    };
    const id = await this.recordingDataManager.createRecording(
        metadata,
        audioData,
    );

    this.recordingSession.value = null;
    return id;
  }

  private async onStopRecording() {
    // TODO(pihsun): Make this function sync since it's called as event handler.
    const id = await this.stopRecording();
    if (id !== null) {
      navigateTo(`/playback?id=${id}`);
    }
  }

  override async connectedCallback(): Promise<void> {
    super.connectedCallback();
    // TODO(pihsun): auto-starting the recording since this page is arrived
    // from clicking "record" button from the main page. Reconsider how to do
    // this properly.
    await this.startRecording();
  }

  override async disconnectedCallback(): Promise<void> {
    super.disconnectedCallback();
    // Cancel current recording when leaving page / hot reloading.
    // TODO: b/336963138 - Have a confirmation before leaving.
    await this.cancelRecording();
  }

  private toggleTranscription() {
    this.showTranscription.update((s) => !s);
  }

  private get deleteDialog(): CraDialog|null {
    const el = this.shadowRoot?.querySelector('#delete-dialog') ?? null;
    if (el === null) {
      return null;
    }
    return assertInstanceof(el, CraDialog);
  }

  private get exitDialog(): CraDialog|null {
    const el = this.shadowRoot?.querySelector('#exit-dialog') ?? null;
    if (el === null) {
      return null;
    }
    return assertInstanceof(el, CraDialog);
  }

  private onDeleteButtonClick() {
    this.deleteDialog?.show();
  }

  private async deleteRecording() {
    // TODO(pihsun): Make this function sync since it's called as event handler.
    await this.cancelRecording();
    navigateTo('/');
  }

  private renderAudioWaveform() {
    if (this.recordingSession.value === null) {
      return nothing;
    }
    const session = this.recordingSession.value;
    return html`
      <audio-waveform .values=${session.progress.value.powers}>
      </audio-waveform>
      <cra-icon-button shape="circle">
        <!-- TODO: b/336963138 - Implement mute. -->
        <cra-icon slot="icon" name="mic"></cra-icon>
      </cra-icon-button>
    `;
  }

  private renderTranscription() {
    if (this.recordingSession.value === null) {
      return nothing;
    }
    // TODO: b/336963138 - Animation while opening/closing the panel.
    const session = this.recordingSession.value;
    return html`<transcription-view
      .textTokens=${session.progress.value.textTokens}
    ></transcription-view>`;
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
      .label=${i18n('Stop recording')}
      @click=${this.onStopRecording}
    >
      <cra-icon slot="leading-icon" name="stop"></cra-icon>
    </cra-button>`;
  }

  private async saveAndExitRecording() {
    await this.stopRecording();
    navigateTo('/');
  }

  private onBackClick() {
    // TODO: b/336963138 - This should directly save and exit when the
    // recording is paused.
    this.exitDialog?.show();
  }

  private closeExitDialog() {
    this.exitDialog?.close();
  }

  private renderExitRecordingDialog() {
    return html`<cra-dialog id="exit-dialog">
      <div slot="headline">${i18n('Exit recording')}</div>
      <div slot="content">
        ${i18n('Leaving this page will end your current recording.')}
      </div>
      <div slot="actions">
        <cra-button
          .label=${i18n('Delete')}
          button-style="secondary"
          @click=${this.deleteRecording}
        ></cra-button>
        <cra-button
          .label=${i18n('Cancel')}
          button-style="secondary"
          @click=${this.closeExitDialog}
        ></cra-button>
        <cra-button
          .label=${i18n('Save and exit')}
          @click=${this.saveAndExitRecording}
        ></cra-button>
      </div>
    </cra-dialog>`;
  }

  private closeDeleteDialog() {
    this.deleteDialog?.close();
  }

  private renderDeleteRecordingDialog() {
    return html`<cra-dialog id="delete-dialog">
      <div slot="headline">${i18n('Delete current recording?')}</div>
      <div slot="actions">
        <cra-button
          .label=${i18n('Cancel')}
          button-style="secondary"
          @click=${this.closeDeleteDialog}
        ></cra-button>
        <cra-button
          .label=${i18n('Delete')}
          @click=${this.deleteRecording}
        ></cra-button>
      </div>
    </cra-dialog>`;
  }

  override render(): RenderResult {
    const mainSectionClasses = {
      'show-transcription': this.showTranscription.value,
    };

    return html`
      <div id="main-area">
        <div id="header" class="sheet">
          <cra-icon-button buttonstyle="floating" @click=${this.onBackClick}>
            <cra-icon slot="icon" name="arrow_back"></cra-icon>
          </cra-icon-button>
          <span id="title">${this.recordingTitle}</span>
          <cra-icon-button
            buttonstyle="toggle"
            @click=${this.toggleTranscription}
          >
            <cra-icon slot="icon" name="notes"></cra-icon>
            <cra-icon slot="selectedIcon" name="notes"></cra-icon>
          </cra-icon-button>
          <cra-icon-button buttonstyle="floating">
            <!-- TODO: b/336963138 - Implements more menu -->
            <cra-icon slot="icon" name="more_vertical"></cra-icon>
          </cra-icon-button>
        </div>
        <div id="middle" class=${classMap(mainSectionClasses)}>
          <div id="audio-waveform-container" class="sheet">
            ${this.renderAudioWaveform()}
          </div>
          <div id="transcription-container" class="sheet">
            ${this.renderTranscription()}
          </div>
        </div>
      </div>
      <div id="footer">
        <div id="timer">${this.renderTimer()}</div>
        <div id="actions">
          <secondary-button @click=${this.onDeleteButtonClick}>
            <cra-icon slot="icon" name="delete"></cra-icon>
          </secondary-button>
          ${this.renderStopRecordButton()}
          <secondary-button>
            <!-- TODO: b/336963138 - Implements pause -->
            <cra-icon slot="icon" name="pause"></cra-icon>
          </secondary-button>
        </div>
      </div>
      ${this.renderDeleteRecordingDialog()} ${this.renderExitRecordingDialog()}
    `;
  }
}

window.customElements.define('record-page', RecordPage);

declare global {
  interface HTMLElementTagNameMap {
    'record-page': RecordPage;
  }
}
