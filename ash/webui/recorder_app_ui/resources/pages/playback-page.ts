// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/menu/menu_separator.js';
import 'chrome://resources/cros_components/slider/slider.js';
import 'chrome://resources/mwc/@material/web/icon/icon.js';
import 'chrome://resources/mwc/@material/web/iconbutton/icon-button.js';
import '../components/audio-waveform.js';
import '../components/cra/cra-image.js';
import '../components/cra/cra-menu-item.js';
import '../components/cra/cra-menu.js';
import '../components/delete-recording-dialog.js';
import '../components/export-dialog.js';
import '../components/recording-file-list.js';
import '../components/recording-info-dialog.js';
import '../components/recording-title.js';
import '../components/secondary-button.js';
import '../components/spoken-message.js';
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
  live,
  nothing,
  PropertyDeclarations,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {CraIconButton} from '../components/cra/cra-icon-button.js';
import {CraMenu} from '../components/cra/cra-menu.js';
import {DeleteRecordingDialog} from '../components/delete-recording-dialog.js';
import {ExportDialog} from '../components/export-dialog.js';
import {RecordingInfoDialog} from '../components/recording-info-dialog.js';
import {RecordingTitle} from '../components/recording-title.js';
import {SummarizationView} from '../components/summarization-view.js';
import {
  AudioPlayerController,
  ReactiveAudio,
} from '../core/audio_player_controller.js';
import {i18n} from '../core/i18n.js';
import {useRecordingDataManager} from '../core/lit/context.js';
import {
  ComputedState,
  ReactiveLitElement,
  ScopedAsyncComputed,
  ScopedEffect,
} from '../core/reactive/lit.js';
import {computed, signal} from '../core/reactive/signal.js';
import {navigateTo} from '../core/state/route.js';
import {
  assert,
  assertExists,
  assertInstanceof,
} from '../core/utils/assert.js';
import {AsyncJobQueue} from '../core/utils/async_job_queue.js';
import {formatDuration} from '../core/utils/datetime.js';
import {InteriorMutableArray} from '../core/utils/interior_mutable_array.js';
import {sleep} from '../core/utils/utils.js';

/**
 * Mapping from playback speed to icon names.
 *
 * Note that the playback speed numbers can be precisely represented by IEEE 754
 * floating point numbers, so using those as key shouldn't pose precision
 * issues.
 */
const PLAYBACK_SPEED_ICON_MAP = new Map([
  [0.25, 'rate_0_25'],
  [0.5, 'rate_0_5'],
  [0.75, 'rate_0_75'],
  [1.0, 'rate_1_0'],
  [1.25, 'rate_1_25'],
  [1.5, 'rate_1_5'],
  [1.75, 'rate_1_75'],
  [2.0, 'rate_2_0'],
]);

const PLAYBACK_SPEEDS = Array.from(PLAYBACK_SPEED_ICON_MAP.keys());

let initialAudio: ReactiveAudio|null = null;

/**
 * Sets the inner audio used for the playback page for continuous playback.
 *
 * This should be called before the playback page is rendered.
 */
export function setInitialAudio(audio: ReactiveAudio|null): void {
  initialAudio = audio;
}

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
      gap: 2px;

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
        position-area: top;
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
      left: 0;
      position: absolute;
    }

    #inline-slider {
      align-items: center;
      display: flex;

      & > cra-icon-button {
        margin-right: 0;
      }

      & > cros-slider {
        min-inline-size: initial;
        width: 180px;
      }

      @container style(--small-viewport: 1) {
        display: none;
      }
    }

    #floating-slider-base {
      display: none;

      @container style(--small-viewport: 1) {
        display: block;
      }

      & > cra-icon-button {
        anchor-name: --volume-button;
        margin: 0;
        padding: 4px;
      }

      /* TODO(pihsun): Animate the opening/closing */
      & > [popover]:popover-open {
        align-items: center;
        background: var(--cros-sys-base_elevated);
        border: none;
        border-radius: 8px;
        box-shadow: var(--cros-sys-app_elevation3);
        display: flex;
        flex-flow: row;
        margin: initial;
        overflow: initial;
        padding: 0;
        position: absolute;
        position-anchor: --volume-button;
        position-area: center span-right;

        & > cros-slider {
          min-inline-size: initial;
          width: 180px;
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

    #menu {
      --cros-menu-width: 200px;
    }

    #speed-menu {
      --cros-menu-width: 160px;
    }
  `;

  static override properties: PropertyDeclarations = {
    recordingId: {type: String},
  };

  recordingId: string|null = null;

  private readonly recordingIdSignal = this.propSignal('recordingId');

  private readonly menu = createRef<CraMenu>();

  private readonly deleteRecordingDialog = createRef<DeleteRecordingDialog>();

  private readonly exportDialog = createRef<ExportDialog>();

  private readonly recordingInfoDialog = createRef<RecordingInfoDialog>();

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly playbackSpeedMenu = createRef<CraMenu>();

  private readonly playbackSpeedMenuOpened = signal(false);

  private readonly floatingVolume = createRef<HTMLElement>();

  private readonly backButton = createRef<CraIconButton>();

  private readonly playPauseButton = createRef<CraIconButton>();

  private readonly transcriptionButtonRef = createRef<CraIconButton>();

  private readonly recordingTitle = createRef<RecordingTitle>();

  private readonly summarizationView = createRef<SummarizationView>();

  private readonly audioPlayer = new AudioPlayerController(
    this,
    this.recordingIdSignal,
  );

  // TODO(pihsun): Loading spinner when loading metadata.
  private readonly recordingMetadata = computed(() => {
    const id = this.recordingIdSignal.value;
    if (id === null) {
      return null;
    }
    return this.recordingDataManager.getMetadata(id).value;
  });

  private readonly transcription = new ScopedAsyncComputed(this, async () => {
    if (this.recordingIdSignal.value === null) {
      return null;
    }
    return this.recordingDataManager.getTranscription(
      this.recordingIdSignal.value,
    );
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

  private readonly spokenAudioTime = signal<number|null>(null);

  private readonly spokenTimeQueue = new AsyncJobQueue('keepLatest');

  protected readonly autoOpenTranscription = new ScopedEffect(this, () => {
    if (this.transcription.state === ComputedState.DONE) {
      // We only updates the transcription open state when it's just
      // computed.
      const transcription = this.transcription.valueSignal.peek();

      // Default to show transcription panel if there's a non-empty
      // transcription.
      this.showTranscription.value =
        transcription !== null && !transcription.isEmpty();
    }
  });

  get pauseButtonForTest(): CraIconButton {
    assert(this.audioPlayer.playing.value, 'The playback is already paused');
    return assertExists(this.playPauseButton.value);
  }

  get backButtonForTest(): CraIconButton {
    return assertExists(this.backButton.value);
  }

  get transcriptionToggleButtonForTest(): CraIconButton {
    return assertExists(this.transcriptionButtonRef.value);
  }

  get recordingTitleForTest(): RecordingTitle {
    return assertExists(this.recordingTitle.value);
  }

  get summarizationViewForTest(): SummarizationView {
    return assertExists(this.summarizationView.value);
  }

  override firstUpdated(): void {
    const backButton = assertExists(this.backButton.value);
    // TODO(pihsun): Autofocus / "default focus" management is reused at
    // multiple places, somehow consolidate the logic and have a derivative for
    // that?
    backButton.updateComplete.then(() => {
      backButton.focus();
    });
  }

  override connectedCallback(): void {
    super.connectedCallback();
    if (initialAudio !== null) {
      this.audioPlayer.setInnerAudio(initialAudio);
      initialAudio = null;
    }
  }

  private onWordClick(ev: CustomEvent<{startMs: number}>) {
    // TODO(pihsun): Currently there's a small time difference between the
    // seeked audio and the real timing of the word being spoken.
    // Investigate if it's from audio playing inaccuracy or from inherit
    // soda event timestamp inaccuracy.
    this.updateAudioTime(ev.detail.startMs / 1000);
  }

  private onPlayPauseClick() {
    this.audioPlayer.togglePlaying();
  }

  private toggleTranscription() {
    this.showTranscription.update((s) => !s);
  }

  private renderAudioWaveform() {
    if (this.powers.value === null) {
      return nothing;
    }
    const currentTime = this.audioPlayer.currentTime.value;
    const currentTimeString = formatDuration(
      {
        seconds: currentTime,
      },
      1,
    );
    return html`
      <div>${currentTimeString}</div>
      <audio-waveform
        .values=${new InteriorMutableArray(this.powers.value)}
        .currentTime=${currentTime}
        .transcription=${this.transcription.value}
      >
      </audio-waveform>
    `;
  }

  private renderTranscription() {
    const transcription = this.transcription.value;
    if (transcription === null) {
      return nothing;
    }
    if (transcription.isEmpty()) {
      return html`<div id="transcription-empty">
        <cra-image name="transcription_no_speech"></cra-image>
        ${i18n.transcriptionNoSpeechText}
      </div>`;
    }
    // TODO: b/336963138 - Animation while opening/closing the panel.
    return html`<transcription-view
      .transcription=${transcription}
      @word-clicked=${this.onWordClick}
      .currentTime=${this.audioPlayer.currentTime.value}
      seekable
    >
      <summarization-view
        .transcription=${transcription}
        ${ref(this.summarizationView)}
      ></summarization-view>
    </transcription-view>`;
  }

  private renderPlayPauseButton() {
    const ariaLabel = this.audioPlayer.playing.value ?
      i18n.playbackPauseButtonTooltip :
      i18n.playbackPlayButtonTooltip;
    return html`<cra-icon-button
      id="play-button"
      shape="circle"
      @click=${this.onPlayPauseClick}
      ${ref(this.playPauseButton)}
      aria-label=${ariaLabel}
    >
      <cra-icon
        slot="icon"
        .name=${this.audioPlayer.playing.value ? 'pause_hero' : 'play_hero'}
      ></cra-icon>
    </cra-icon-button>`;
  }

  private onTimelineInput(ev: Event) {
    const target = assertInstanceof(ev.target, CrosSlider);
    this.audioPlayer.currentTime.value = target.value;
  }

  /**
   * Update the audio time and make the screen reader read the updated time.
   *
   * @param updatedTime Updated audio player time.
   */
  private updateAudioTime(updatedTime: number) {
    this.audioPlayer.currentTime.value = updatedTime;

    // The spoken feedback can still be spoken by screen reader even though it's
    // not focusable, so remove the time status as soon as possible.
    this.spokenTimeQueue.push(async () => {
      this.spokenAudioTime.value = updatedTime;
      await sleep(100);
      this.spokenAudioTime.value = null;
    });
  }

  private onForward10Secs() {
    this.updateAudioTime(this.audioPlayer.currentTime.value + 10);
  }

  private onRewind10Secs() {
    this.updateAudioTime(this.audioPlayer.currentTime.value - 10);
  }

  private onDeleteClick() {
    this.deleteRecordingDialog.value?.show();
  }

  private deleteRecording() {
    if (this.recordingId !== null) {
      this.recordingDataManager.remove(this.recordingId);
      navigateTo('main');
    }
  }

  private onExportClick() {
    this.exportDialog.value?.show();
  }

  private onShowDetailClick() {
    this.recordingInfoDialog.value?.show();
  }

  private renderMenu() {
    // TODO: b/344789992 - Implements show detail.
    return html`
      <cra-menu ${ref(this.menu)} anchor="show-menu" id="menu">
        <cra-menu-item
          headline=${i18n.playbackMenuExportOption}
          @cros-menu-item-triggered=${this.onExportClick}
        ></cra-menu-item>
        <cra-menu-item
          headline=${i18n.playbackMenuShowDetailOption}
          @cros-menu-item-triggered=${this.onShowDetailClick}
        ></cra-menu-item>
        <cros-menu-separator></cros-menu-separator>
        <cra-menu-item
          headline=${i18n.playbackMenuDeleteOption}
          @cros-menu-item-triggered=${this.onDeleteClick}
        ></cra-menu-item>
      </cra-menu>
      <recording-info-dialog
        ${ref(this.recordingInfoDialog)}
        .recordingId=${this.recordingId}
      ></recording-info-dialog>
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
    const transcriptionLabel = this.showTranscription.value ?
      i18n.playbackHideTranscriptButtonTooltip :
      i18n.playbackShowTranscriptButtonTooltip;
    const transcriptionToggleButton =
      this.transcription.value === null ? nothing : html`
            <cra-icon-button
              buttonstyle="toggle"
              .selected=${live(this.showTranscription.value)}
              @click=${this.toggleTranscription}
              ${ref(this.transcriptionButtonRef)}
              aria-label=${transcriptionLabel}
            >
              <cra-icon slot="icon" name="notes"></cra-icon>
              <cra-icon slot="selectedIcon" name="notes"></cra-icon>
            </cra-icon-button>
          `;

    return html`
      <div id="header" class="sheet">
        <cra-icon-button
          buttonstyle="floating"
          @click=${() => navigateTo('main')}
          ${ref(this.backButton)}
          aria-label=${i18n.backToMainButtonAriaLabel}
        >
          <cra-icon slot="icon" name="arrow_back"></cra-icon>
        </cra-icon-button>
        <recording-title
          .recordingMetadata=${this.recordingMetadata.value}
          ${ref(this.recordingTitle)}
        >
        </recording-title>
        ${transcriptionToggleButton}
        <cra-icon-button
          buttonstyle="floating"
          id="show-menu"
          @click=${this.toggleMenu}
          aria-label=${i18n.playbackMenuButtonTooltip}
        >
          <cra-icon slot="icon" name="more_vertical"></cra-icon>
        </cra-icon-button>
        ${this.renderMenu()}
      </div>
    `;
  }

  private renderTimeSpokenStatus() {
    const spokenTime = this.spokenAudioTime.value;

    if (spokenTime === null) {
      return nothing;
    }

    return html`<spoken-message aria-live="polite" role="status">
      ${formatDuration({seconds: spokenTime}, 1)}
    </spoken-message>`;
  }

  private renderAudioTimeline() {
    if (this.recordingMetadata.value === null) {
      return nothing;
    }
    const currentTime = this.audioPlayer.currentTime.value;

    const currentTimeString = formatDuration({
      seconds: currentTime,
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
        .value=${currentTime}
        @input=${this.onTimelineInput}
        aria-label=${i18n.playbackSeekSliderAriaLabel}
      ></cros-slider>
      <div>
        <span>${currentTimeString}</span>
        <span>${totalTimeString}</span>
      </div>
      ${this.renderTimeSpokenStatus()}
    </div>`;
  }

  private getSpeedLabel(speed: number): string {
    return speed === 1.0 ? i18n.playbackSpeedNormalOption : speed.toString();
  }

  private getPlaybackSpeedControlLabel(): string {
    return `${i18n.playbackSpeedButtonTooltip}: ${
      this.getSpeedLabel(this.audioPlayer.playbackSpeed.value)}`;
  }

  private renderSpeedControl(): RenderResult {
    const iconName = assertExists(
      PLAYBACK_SPEED_ICON_MAP.get(this.audioPlayer.playbackSpeed.value),
    );
    const menuItems = PLAYBACK_SPEEDS.map((speed) => {
      const label = this.getSpeedLabel(speed);
      const onClick = () => {
        this.audioPlayer.playbackSpeed.value = speed;
      };

      return html`<cra-menu-item
        headline=${label}
        ?checked=${this.audioPlayer.playbackSpeed.value === speed}
        @cros-menu-item-triggered=${onClick}
        data-role="menuitemradio"
      ></cra-menu-item>`;
    });

    const onMenuOpen = () => {
      this.playbackSpeedMenuOpened.value = true;
    };
    const onMenuClose = () => {
      this.playbackSpeedMenuOpened.value = false;
    };
    const togglePlaybackSpeedMenu = () => {
      this.playbackSpeedMenu.value?.toggle();
    };

    const classes = {
      selected: this.playbackSpeedMenuOpened.value,
    };
    return html`
      <cra-menu
        ${ref(this.playbackSpeedMenu)}
        anchor="show-speed-menu"
        id="speed-menu"
        @opened=${onMenuOpen}
        @closed=${onMenuClose}
      >
        ${menuItems}
      </cra-menu>
      <cra-icon-button
        buttonstyle="filled"
        class="with-toggle-style ${classMap(classes)}"
        id="show-speed-menu"
        @click=${togglePlaybackSpeedMenu}
        aria-haspopup="true"
        aria-label=${this.getPlaybackSpeedControlLabel()}
      >
        <cra-icon slot="icon" .name=${iconName}></cra-icon>
      </cra-icon-button>
    `;
  }

  private onVolumeInput(ev: Event) {
    const slider = assertInstanceof(ev.target, CrosSlider);
    this.audioPlayer.muted.value = false;
    this.audioPlayer.volume.value = slider.value / 100;
    this.requestUpdate();
  }

  private toggleMuted() {
    this.audioPlayer.muted.update((s) => !s);
    this.requestUpdate();
  }

  private renderVolumeIcon() {
    const {muted, volume} = this.audioPlayer;
    const iconName = (() => {
      if (muted.value) {
        return 'volume_mute';
      }
      if (volume.value === 0) {
        return 'volume_off';
      }
      return volume.value < 0.5 ? 'volume_down' : 'volume_up';
    })();
    return html`<cra-icon slot="icon" .name=${iconName}></cra-icon>`;
  }

  private renderVolumeSlider() {
    const {muted, volume} = this.audioPlayer;
    const volumeDisplay = muted.value ? 0 : Math.round(volume.value * 100);
    return html`
      <cros-slider
        withlabel
        .value=${volumeDisplay}
        min="0"
        max="100"
        @input=${this.onVolumeInput}
        aria-label=${i18n.playbackVolumeAriaLabel}
      ></cros-slider>
    `;
  }

  private showFloatingVolume() {
    this.floatingVolume.value?.showPopover();
  }

  private hideFloatingVolume(ev: FocusEvent) {
    const newTarget = ev.relatedTarget;
    if (newTarget !== null && newTarget instanceof Node &&
        this.floatingVolume.value?.contains(newTarget)) {
      return;
    }
    this.floatingVolume.value?.hidePopover();
  }

  private renderVolumeControl(): RenderResult {
    return html`
      <div id="inline-slider">
        <cra-icon-button
          buttonstyle="toggle"
          @click=${this.toggleMuted}
          aria-label=${i18n.playbackMuteButtonTooltip}
          class="with-floating-style"
          .selected=${this.audioPlayer.muted.value}
        >
          ${this.renderVolumeIcon()}
          <cra-icon slot="selectedIcon" name="volume_mute"></cra-icon>
        </cra-icon-button>
        ${this.renderVolumeSlider()}
      </div>
      <div id="floating-slider-base">
        <cra-icon-button
          buttonstyle="floating"
          @click=${this.showFloatingVolume}
          aria-label=${i18n.playbackVolumeAriaLabel}
        >
          ${this.renderVolumeIcon()}
        </cra-icon-button>
        <div
          popover
          ${ref(this.floatingVolume)}
          @focusout=${this.hideFloatingVolume}
        >
          <cra-icon-button buttonstyle="floating" @click=${this.toggleMuted}>
            ${this.renderVolumeIcon()}
          </cra-icon-button>
          ${this.renderVolumeSlider()}
          <cra-icon-button
            buttonstyle="floating"
            @click=${this.hideFloatingVolume}
          >
            <cra-icon slot="icon" name="close"></cra-icon>
          </cra-icon-button>
        </div>
      </div>
    `;
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
          <div
            id="transcription-container"
            class="sheet"
            aria-label=${i18n.playbackTranscriptLandmarkAriaLabel}
            role="region"
          >
            ${this.renderTranscription()}
          </div>
        </div>
      </div>
      <div
        id="footer"
        aria-label=${i18n.playbackControlsLandmarkAriaLabel}
        role="complementary"
      >
        ${this.renderAudioTimeline()}
        <div id="actions">
          <div id="volume-controls">${this.renderVolumeControl()}</div>
          <div id="middle-controls">
            <secondary-button
              @click=${this.onRewind10Secs}
              aria-label=${i18n.playbackBackwardButtonTooltip}
            >
              <cra-icon slot="icon" name="replay_10"></cra-icon>
            </secondary-button>
            ${this.renderPlayPauseButton()}
            <secondary-button
              @click=${this.onForward10Secs}
              aria-label=${i18n.playbackForwardButtonTooltip}
            >
              <cra-icon slot="icon" name="forward_10"></cra-icon>
            </secondary-button>
          </div>
          <div id="speed-controls">${this.renderSpeedControl()}</div>
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
