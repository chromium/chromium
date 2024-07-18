// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/menu/menu_separator.js';
import 'chrome://resources/cros_components/slider/slider.js';
import 'chrome://resources/mwc/@material/web/icon/icon.js';
import 'chrome://resources/mwc/@material/web/iconbutton/icon-button.js';
import '../components/audio-waveform.js';
import '../components/cra/cra-image.js';
import '../components/cra/cra-menu.js';
import '../components/cra/cra-menu-item.js';
import '../components/delete-recording-dialog.js';
import '../components/export-dialog.js';
import '../components/recording-title.js';
import '../components/recording-file-list.js';
import '../components/secondary-button.js';
import '../components/summarization-view.js';
import '../components/transcription-view.js';

import {
  Slider as CrosSlider,
} from 'chrome://resources/cros_components/slider/slider.js';
import {
  classMap,
  createRef,
  css,
  html,
  nothing,
  PropertyDeclarations,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {CraMenu} from '../components/cra/cra-menu.js';
import {DeleteRecordingDialog} from '../components/delete-recording-dialog.js';
import {ExportDialog} from '../components/export-dialog.js';
import {i18n} from '../core/i18n.js';
import {
  AnimationFrameController,
} from '../core/lit/animation_frame_controller.js';
import {useRecordingDataManager} from '../core/lit/context.js';
import {
  ReactiveLitElement,
  ScopedAsyncComputed,
  ScopedAsyncEffect,
} from '../core/reactive/lit.js';
import {computed, signal} from '../core/reactive/signal.js';
import {navigateTo} from '../core/state/route.js';
import {assertInstanceof} from '../core/utils/assert.js';
import {formatDuration} from '../core/utils/datetime.js';

/**
 * Playback page of Recorder App.
 */
export class PlaybackPage extends ReactiveLitElement {
  static override styles = css`
    :host {
      background-color: var(--cros-sys-app_base_shaded);
      box-sizing: border-box;
      display: flex;
      flex-flow: column;
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
      background-color: var(--cros-sys-app_base);
      border-radius: 4px;
    }

    #header {
      align-items: center;
      display: flex;
      flex-flow: row;
      padding: 8px;

      & > recording-title {
        margin: 0 auto 0 -4px;
      }
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

    #transcription-empty {
      align-items: center;
      display: flex;
      flex-flow: column;
      font: var(--cros-headline-1-font);
      gap: 16px;
      height: 100%;
      justify-content: center;
      width: 100%;
    }

    audio-waveform,
    transcription-view {
      box-sizing: border-box;
      height: 100%;
      width: 100%;
    }

    #audio-waveform-container {
      align-items: center;
      display: flex;
      flex-flow: column;
      justify-content: center;

      & > div {
        color: var(--cros-sys-on_surface_variant);
        font: 440 24px/32px var(--monospace-font-family);
        letter-spacing: 0.03em;
        margin-bottom: 4px;
        position: absolute;
        position-anchor: --waveform;
        inset-area: top;
      }
    }

    audio-waveform {
      height: 180px;
      anchor-name: --waveform;
    }

    #footer {
      align-items: stretch;
      display: flex;
      flex-flow: column;
      gap: 16px;
      padding: 16px 8px;
    }

    #audio-timeline {
      align-items: stretch;
      display: flex;
      flex-flow: column;

      & > div {
        display: flex;
        flex-flow: row;
        font: var(--cros-body-2-font);
        justify-content: space-between;
        padding: 0 16px;
      }
    }

    #actions {
      align-items: center;
      display: flex;
      justify-content: center;
      position: relative;
    }

    #volume-controls {
      align-items: center;
      display: flex;
      left: 0;
      position: absolute;

      & > cra-icon-button {
        margin-right: 0;
      }

      & > cros-slider {
        min-inline-size: initial;
        width: 180px;

        /*
         * TODO: b/336963138 - Slider should be in a separate layer when
         * viewpoint is small.
         */
        @container style(--small-viewport: 1) {
          display: none;
        }
      }
    }

    #middle-controls {
      align-items: center;
      display: flex;
      flex-flow: row;
      gap: 24px;
    }

    #speed-controls {
      position: absolute;
      right: 0;
    }

    #play-button {
      --cra-icon-button-container-color: var(--cros-sys-primary);
      --cra-icon-button-container-height: 96px;
      --cra-icon-button-container-width: 152px;
      --cra-icon-button-hover-state-layer-color: var(
        --cros-sys-hover_on_prominent
      );
      --cra-icon-button-pressed-state-layer-color: var(
        --cros-sys-ripple_primary
      );
      --cros-icon-button-color-override: var(--cros-sys-on_primary);
      --cros-icon-button-icon-size: 32px;

      @container style(--small-viewport: 1) {
        --cra-icon-button-container-height: 80px;
        --cra-icon-button-container-width: 136px;
      }

      margin: 0;
    }

    summarization-view {
      padding: 0 12px;
    }

    cra-menu {
      --cros-menu-width: 200px;
    }
  `;

  static override properties: PropertyDeclarations = {
    recordingId: {type: String},
  };

  recordingId: string|null = null;

  private readonly recordingIdSignal = this.propSignal('recordingId');

  private latestAudioSrc: string|null = null;

  private readonly currentTime = signal(0);

  private readonly audioPlaying = signal(false);

  private readonly audio = new Audio();

  private readonly menu = createRef<CraMenu>();

  private readonly deleteRecordingDialog = createRef<DeleteRecordingDialog>();

  private readonly exportDialog = createRef<ExportDialog>();

  private readonly recordingDataManager = useRecordingDataManager();

  // TODO(pihsun): Loading spinner when loading metadata.
  private readonly recordingMetadata = computed(() => {
    const id = this.recordingIdSignal.value;
    if (id === null) {
      return null;
    }
    return this.recordingDataManager.getMetadata(id).value;
  });

  // This is marked as protected to suppress the unused member error.
  protected readonly autoplay = new ScopedAsyncEffect(this, async (signal) => {
    const id = this.recordingIdSignal.value;
    if (id === null) {
      return;
    }
    const data = await this.recordingDataManager.getAudioFile(id);
    signal.throwIfAborted();
    this.revokeAudio();
    this.latestAudioSrc = URL.createObjectURL(data);
    this.audio.src = this.latestAudioSrc;
    this.audio.load();
    await this.audio.play();
  });

  private readonly textTokens = new ScopedAsyncComputed(this, async () => {
    if (this.recordingIdSignal.value === null) {
      return null;
    }
    const {textTokens} = await this.recordingDataManager.getTranscription(
      this.recordingIdSignal.value,
    );
    return textTokens;
  });

  private readonly powers = new ScopedAsyncComputed(this, async () => {
    if (this.recordingIdSignal.value === null) {
      return null;
    }
    const {powers} = await this.recordingDataManager.getAudioPower(
      this.recordingIdSignal.value,
    );
    return powers;
  });

  private readonly showTranscription = signal(false);

  constructor() {
    super();

    // TODO(pihsun): These will look much better as decorator...
    this.addController(
      new AnimationFrameController(() => {
        // We use AnimationFrameController instead of the timeupdate event
        // since timeupdate fires infrequently and doesn't look smooth while
        // playing.
        // TODO(shik): Pause/Resume the animation frame loop properly,
        // especially when the audio is fully played and stopped so we
        // won't
        // keep the audio stream open as the audio server would be kept
        // awake as well.
        this.currentTime.value = this.audio.currentTime;
        this.audioPlaying.value = !this.audio.paused;
      }),
    );
  }

  private revokeAudio() {
    if (this.latestAudioSrc !== null) {
      URL.revokeObjectURL(this.latestAudioSrc);
      this.latestAudioSrc = null;
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.audio.pause();
    this.revokeAudio();
  }

  private onWordClick(ev: CustomEvent<{startMs: number}>) {
    // TODO(pihsun): Currently there's a small time difference between the
    // seeked audio and the real timing of the word being spoken.
    // Investigate if it's from audio playing inaccuracy or from inherit
    // soda event timestamp inaccuracy.
    this.audio.currentTime = ev.detail.startMs / 1000;
  }

  private onPlayPauseClick() {
    if (this.audio.paused) {
      // TODO(pihsun): This is async, should we await it?
      void this.audio.play();
    } else {
      this.audio.pause();
    }
  }

  private toggleTranscription() {
    this.showTranscription.update((s) => !s);
  }

  private renderAudioWaveform() {
    if (this.powers.value === null) {
      return nothing;
    }
    const recordingLength = formatDuration(
      {
        seconds: this.currentTime.value,
      },
      1,
    );
    return html`
      <div>${recordingLength}</div>
      <audio-waveform
        .values=${this.powers.value}
        .currentTime=${this.currentTime.value}
      >
      </audio-waveform>
    `;
  }

  private renderTranscription() {
    const textTokens = this.textTokens.value;
    if (textTokens === null) {
      return nothing;
    }
    if (textTokens.length === 0) {
      return html`<div id="transcription-empty">
        <cra-image name="transcription_no_speech"></cra-image>
        ${i18n.transcriptionNoSpeechText}
      </div>`;
    }
    // TODO: b/336963138 - Animation while opening/closing the panel.
    return html`<transcription-view
      .textTokens=${textTokens}
      @word-clicked=${this.onWordClick}
      .currentTime=${this.currentTime.value}
      seekable
    >
      <summarization-view .textTokens=${textTokens}></summarization-view>
    </transcription-view>`;
  }

  private renderPlayPauseButton() {
    return html`<cra-icon-button
      id="play-button"
      shape="circle"
      @click=${this.onPlayPauseClick}
    >
      <cra-icon
        slot="icon"
        .name=${this.audioPlaying.value ? 'pause_hero' : 'play_hero'}
      ></cra-icon>
    </cra-icon-button>`;
  }

  private onTimelineInput(ev: Event) {
    const target = assertInstanceof(ev.target, CrosSlider);
    this.audio.currentTime = target.value;
  }

  private onForward10Secs() {
    this.audio.currentTime += 10;
  }

  private onRewind10Secs() {
    this.audio.currentTime -= 10;
  }

  private onDeleteClick() {
    this.deleteRecordingDialog.value?.show();
  }

  private deleteRecording() {
    if (this.recordingId !== null) {
      this.recordingDataManager.remove(this.recordingId);
      navigateTo('/');
    }
  }

  private onExportClick() {
    this.exportDialog.value?.show();
  }

  private renderMenu() {
    // TODO: b/344789992 - Implements show detail.
    return html`
      <cra-menu ${ref(this.menu)} anchor="show-menu">
        <cra-menu-item
          headline=${i18n.playbackMenuExportOption}
          @cros-menu-item-triggered=${this.onExportClick}
        ></cra-menu-item>
        <cra-menu-item
          headline=${i18n.playbackMenuShowDetailOption}
        ></cra-menu-item>
        <cros-menu-separator></cros-menu-separator>
        <cra-menu-item
          headline=${i18n.playbackMenuDeleteOption}
          @cros-menu-item-triggered=${this.onDeleteClick}
        ></cra-menu-item>
      </cra-menu>
      <export-dialog ${ref(this.exportDialog)} .recordingId=${this.recordingId}>
      </export-dialog>
      <delete-recording-dialog
        ${ref(this.deleteRecordingDialog)}
        @delete=${this.deleteRecording}
      >
      </delete-recording-dialog>
    `;
  }

  private toggleMenu() {
    this.menu.value?.toggle();
  }

  private renderHeader() {
    const transcriptionToggleButton =
      this.textTokens.value === null ? nothing : html`
            <cra-icon-button
              buttonstyle="toggle"
              @click=${this.toggleTranscription}
            >
              <cra-icon slot="icon" name="notes"></cra-icon>
              <cra-icon slot="selectedIcon" name="notes"></cra-icon>
            </cra-icon-button>
          `;

    return html`
      <div id="header" class="sheet">
        <cra-icon-button buttonstyle="floating" href="/">
          <cra-icon slot="icon" name="arrow_back"></cra-icon>
        </cra-icon-button>
        <recording-title .recordingMetadata=${this.recordingMetadata.value}>
        </recording-title>
        ${transcriptionToggleButton}
        <cra-icon-button
          buttonstyle="floating"
          id="show-menu"
          @click=${this.toggleMenu}
        >
          <!-- TODO: b/336963138 - Implements more menu -->
          <cra-icon slot="icon" name="more_vertical"></cra-icon>
        </cra-icon-button>
      </div>
      ${this.renderMenu()}
    `;
  }

  private renderAudioTimeline() {
    if (this.recordingMetadata.value === null) {
      return nothing;
    }

    const currentTimeString = formatDuration({
      seconds: this.currentTime.value,
    });
    const totalTimeString = formatDuration({
      milliseconds: this.recordingMetadata.value.durationMs,
    });

    // TODO(pihsun): The "step" variable controls both the smallest unit of the
    // slider, and what left/right key would step backward/forward, but we
    // might want the step of left/right key to be larger than the precision
    // of dragging the slider?
    return html`<div id="audio-timeline">
      <cros-slider
        min="0"
        max=${this.recordingMetadata.value.durationMs / 1000}
        step="0.1"
        .value=${this.currentTime.value}
        @input=${this.onTimelineInput}
      ></cros-slider>
      <div>
        <span>${currentTimeString}</span>
        <span>${totalTimeString}</span>
      </div>
    </div>`;
  }

  override render(): RenderResult {
    const mainSectionClasses = {
      'show-transcription': this.showTranscription.value,
    };
    return html`
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
      <div id="footer">
        ${this.renderAudioTimeline()}
        <div id="actions">
          <div id="volume-controls">
            <cra-icon-button buttonstyle="floating">
              <!-- TODO: b/336963138 - Implements volume button -->
              <!--
                TODO: b/336963138 - The icon should change based on current
                volume.
              -->
              <cra-icon slot="icon" name="volume_up"></cra-icon>
            </cra-icon-button>
            <!-- TODO: b/336963138 - Implements volume slider -->
            <cros-slider withlabel value="50" min="0" max="100"></cros-slider>
          </div>
          <div id="middle-controls">
            <secondary-button @click=${this.onRewind10Secs}>
              <cra-icon slot="icon" name="replay_10"></cra-icon>
            </secondary-button>
            ${this.renderPlayPauseButton()}
            <secondary-button @click=${this.onForward10Secs}>
              <cra-icon slot="icon" name="forward_10"></cra-icon>
            </secondary-button>
          </div>
          <div id="speed-controls">
            <cra-icon-button buttonstyle="floating">
              <!-- TODO: b/336963138 - Implements speed control -->
              <cra-icon slot="icon" name="rate_1_0"></cra-icon>
            </cra-icon-button>
          </div>
        </div>
      </div>
    `;
  }
}

window.customElements.define('playback-page', PlaybackPage);

declare global {
  interface HTMLElementTagNameMap {
    'playback-page': PlaybackPage;
  }
}
