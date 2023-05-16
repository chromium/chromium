// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertExists,
  assertInstanceof,
} from '../../assert.js';
import * as dom from '../../dom.js';
import {reportError} from '../../error.js';
import * as expert from '../../expert.js';
import * as h264 from '../../h264.js';
import {I18nString} from '../../i18n_string.js';
import {LowStorageActionType, sendLowStorageEvent} from '../../metrics.js';
import {Filenamer} from '../../models/file_namer.js';
import * as loadTimeData from '../../models/load_time_data.js';
import {
  GifSaver,
  TimeLapseSaver,
  VideoSaver,
} from '../../models/video_saver.js';
import {ChromeHelper} from '../../mojo/chrome_helper.js';
import {DeviceOperator} from '../../mojo/device_operator.js';
import {CrosImageCapture} from '../../mojo/image_capture.js';
import {StorageMonitorStatus} from '../../mojo/type.js';
import * as sound from '../../sound.js';
import * as state from '../../state.js';
import * as toast from '../../toast.js';
import {
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,
  getVideoTrackSettings,
  LowStorageError,
  Metadata,
  NoChunkError,
  NoFrameError,
  PreviewVideo,
  Resolution,
  VideoType,
} from '../../type.js';
import {getFpsRangeFromConstraints} from '../../util.js';
import {WaitableEvent} from '../../waitable_event.js';
import {StreamConstraints} from '../stream_constraints.js';
import {StreamManager} from '../stream_manager.js';
import {StreamManagerChrome} from '../stream_manager_chrome.js';

import {ModeBase, ModeFactory} from './mode_base.js';
import {PhotoResult} from './photo.js';
import {GifRecordTime, RecordTime} from './record_time.js';

/**
 * Maps from board name to its default encoding profile and bitrate multiplier.
 */
const encoderPreference = new Map([
  ['corsola', {profile: h264.Profile.HIGH, multiplier: 6}],
  ['strongbad', {profile: h264.Profile.HIGH, multiplier: 6}],
  ['trogdor', {profile: h264.Profile.HIGH, multiplier: 6}],
  ['dedede', {profile: h264.Profile.HIGH, multiplier: 8}],
  ['volteer', {profile: h264.Profile.HIGH, multiplier: 8}],
]);

let avc1Parameters: h264.EncoderParameters|null = null;

/**
 * The minimum duration of videos captured via cca.
 */
const MINIMUM_VIDEO_DURATION_IN_MILLISECONDS = 500;

/**
 * The maximal length of the longer side of gif width or height.
 */
const GIF_MAX_SIDE = 640;

/**
 * Maximum recording time for GIF animation mode.
 */
const MAX_GIF_DURATION_MS = 5000;

/**
 * Sample ratio of grabbing gif frame to be encoded.
 */
const GRAB_GIF_FRAME_RATIO = 2;

/**
 * Initial speed for time-lapse recording.
 */
export const TIME_LAPSE_INITIAL_SPEED = 5;

/**
 * Minimum bitrate multiplier for time lapse recording.
 */
const TIME_LAPSE_MIN_BITRATE_MULTIPLIER = 6;

/**
 * Sets avc1 parameter used in video recording.
 */
export function setAvc1Parameters(params: h264.EncoderParameters|null): void {
  avc1Parameters = params;
}

/**
 * Generate AVC suffix string from given h264 params.
 *
 * @return Suffix string for AVC codecs.
 */
function getAVCSuffix(param: h264.EncoderParameters) {
  const {profile, level} = param;
  return '.' + profile.toString(16).padStart(2, '0') +
      level.toString(16).padStart(4, '0');
}

/**
 * Gets video recording MIME type. Mkv with AVC1 is the only preferred format.
 *
 * @return Video recording MIME type.
 */
function getVideoMimeType(param: h264.EncoderParameters|null): string {
  const suffix = param !== null ? getAVCSuffix(param) : '';
  return `video/x-matroska;codecs=avc1${suffix},pcm`;
}

/**
 * Gets VideoEncoder's config from current h264 params and resolutions.
 *
 * @return VideoEncoderConfig.
 */
function getVideoEncoderConfig(
    param: h264.EncoderParameters, resolution: Resolution): VideoEncoderConfig {
  const suffix = getAVCSuffix(param);
  return {
    codec: `avc1${suffix}`,
    width: resolution.width,
    height: resolution.height,
    bitrate: param.bitrate,
    bitrateMode: 'constant',
    avc: {format: 'annexb'},
  };
}


/**
 * The 'beforeunload' listener which will show confirm dialog when trying to
 * close window.
 */
function beforeUnloadListener(event: BeforeUnloadEvent) {
  event.preventDefault();
  event.returnValue = '';
}

export interface VideoResult {
  autoStopped: boolean;
  resolution: Resolution;
  duration: number;
  videoSaver: VideoSaver;
  everPaused: boolean;
}

export interface GifResult {
  name: string;
  gifSaver: GifSaver;
  resolution: Resolution;
  duration: number;
}

export interface TimeLapseResult {
  autoStopped: boolean;
  duration: number;
  everPaused: boolean;
  resolution: Resolution;
  speed: number;
  timeLapseSaver: TimeLapseSaver;
}

/**
 * Provides functions with external dependency used by video mode and handles
 * the captured result video.
 */
export interface VideoHandler {
  /**
   * Creates VideoSaver to save video capture result.
   */
  createVideoSaver(): Promise<VideoSaver>;

  /**
   * Handles the result video snapshot.
   */
  handleVideoSnapshot(videoSnapshotResult: PhotoResult): Promise<void>;

  /**
   * Plays UI effect when doing video snapshot.
   */
  playShutterEffect(): void;

  onGifCaptureDone(gifResult: GifResult): Promise<void>;

  onVideoCaptureDone(videoResult: VideoResult): Promise<void>;

  onTimeLapseCaptureDone(timeLapseResult: TimeLapseResult): Promise<void>;
}

// This is used as an enum.
/* eslint-disable @typescript-eslint/naming-convention */
const RecordType = {
  NORMAL: state.State.RECORD_TYPE_NORMAL,
  GIF: state.State.RECORD_TYPE_GIF,
  TIME_LAPSE: state.State.RECORD_TYPE_TIME_LAPSE,
} as const;
/* eslint-enable @typescript-eslint/naming-convention */

type RecordType = typeof RecordType[keyof typeof RecordType];

/**
 * Video mode capture controller.
 */
export class Video extends ModeBase {
  private readonly captureResolution: Resolution;

  private captureStream: MediaStream|null = null;

  /**
   * MediaRecorder object to record motion pictures.
   */
  private mediaRecorder: MediaRecorder|null = null;

  /**
   * CrosImageCapture object which is used to grab video snapshot from the
   * recording stream.
   */
  private recordingImageCapture: CrosImageCapture|null = null;

  /**
   * Record-time for the elapsed recording time.
   */
  private readonly recordTime = new RecordTime();

  /**
   * Record-time for the elapsed gif recording time.
   */
  private readonly gifRecordTime: GifRecordTime;

  /**
   * Record type of ongoing recording.
   */
  private recordingType: RecordType = RecordType.NORMAL;

  /**
   * The ongoing video snapshot.
   */
  private snapshotting: Promise<void>|null = null;

  /**
   * Promise for process of toggling video pause/resume. Sets to null if CCA
   * is already paused or resumed.
   */
  private togglePausedInternal: Promise<void>|null = null;

  /**
   * Whether current recording ever paused/resumed before it ended.
   */
  private everPaused = false;

  /**
   * Whether the current recording is auto stopped due to low storage.
   */
  private autoStopped = false;

  /**
   * Whether the current recording should be stopped.
   */
  private stopped = false;

  /**
   * HTMLElement displaying warning about low storage.
   */
  private readonly lowStorageWarningNudge = dom.get('#nudge', HTMLDivElement);

  constructor(
      video: PreviewVideo,
      private readonly captureConstraints: StreamConstraints|null,
      captureResolution: Resolution|null,
      private readonly snapshotResolution: Resolution|null,
      facing: Facing,
      private readonly handler: VideoHandler,
      private readonly frameRate: number,
  ) {
    super(video, facing);

    this.captureResolution = (() => {
      if (captureResolution !== null) {
        return captureResolution;
      }
      const {width, height} = video.getVideoSettings();
      return new Resolution(width, height);
    })();

    this.gifRecordTime = new GifRecordTime(
        {maxTime: MAX_GIF_DURATION_MS, onMaxTimeout: () => this.stop()});
  }

  override async clear(): Promise<void> {
    await this.stopCapture();
    if (this.captureStream !== null) {
      if (expert.isEnabled(expert.ExpertOption.ENABLE_MULTISTREAM_RECORDING)) {
        await StreamManager.getInstance().closeCaptureStream(
            this.captureStream);
      } else if (expert.isEnabled(
                     expert.ExpertOption.ENABLE_MULTISTREAM_RECORDING_CHROME)) {
        StreamManagerChrome.getInstance().stopCaptureStream();
      }
      this.captureStream = null;
    }
  }

  /**
   * @return Returns record type of checked radio buttons in record type option
   *     groups.
   */
  private getToggledRecordOption(): RecordType {
    if (state.get(state.State.SHOULD_HANDLE_INTENT_RESULT)) {
      return RecordType.NORMAL;
    }
    return Object.values(RecordType).find((t) => state.get(t)) ??
        RecordType.NORMAL;
  }

  /**
   * @return Returns whether taking video sanpshot via Blob stream is enabled.
   */
  async isBlobVideoSnapshotEnabled(): Promise<boolean> {
    const deviceOperator = DeviceOperator.getInstance();
    const {deviceId} = this.video.getVideoSettings();
    return deviceOperator !== null &&
        (await deviceOperator.isBlobVideoSnapshotEnabled(deviceId));
  }

  /**
   * Takes a video snapshot during recording.
   *
   * @return Promise resolved when video snapshot is finished.
   */
  async takeSnapshot(): Promise<void> {
    if (this.snapshotting !== null) {
      return;
    }
    state.set(state.State.SNAPSHOTTING, true);
    this.snapshotting = (async () => {
      try {
        const timestamp = Date.now();
        let blob: Blob;
        let metadata: Metadata|null = null;
        if (await this.isBlobVideoSnapshotEnabled()) {
          assert(this.snapshotResolution !== null);
          const photoSettings: PhotoSettings = {
            imageWidth: this.snapshotResolution.width,
            imageHeight: this.snapshotResolution.height,
          };
          const results = await this.getImageCapture().takePhoto(photoSettings);
          blob = await results[0].pendingBlob;
          metadata = await results[0].pendingMetadata;
        } else {
          blob = await assertInstanceof(
                     this.recordingImageCapture, CrosImageCapture)
                     .grabJpegFrame();
        }

        this.handler.playShutterEffect();
        await this.handler.handleVideoSnapshot({
          blob,
          resolution: this.captureResolution,
          timestamp,
          metadata,
        });
      } finally {
        state.set(state.State.SNAPSHOTTING, false);
        this.snapshotting = null;
      }
    })();
    return this.snapshotting;
  }

  private toggleLowStorageWarning(show: boolean): void {
    if (show) {
      sendLowStorageEvent(LowStorageActionType.SHOW_WARNING_MSG);
    }
    this.lowStorageWarningNudge.hidden = !show;
  }

  /**
   * Start monitor storage status and return initial status.
   *
   * @return Promise resolved to boolean indicating whether users can
   * start/resume the recording.
   */
  private async startMonitorStorage(): Promise<boolean> {
    const onChange = (newState: StorageMonitorStatus) => {
      if (newState === StorageMonitorStatus.NORMAL) {
        this.toggleLowStorageWarning(false);
      } else if (newState === StorageMonitorStatus.LOW) {
        this.toggleLowStorageWarning(true);
      } else if (newState === StorageMonitorStatus.CRITICALLY_LOW) {
        if (!state.get(state.State.RECORDING_PAUSED)) {
          this.autoStopped = true;
          this.stop();
        } else {
          this.toggleLowStorageWarning(true);
        }
      }
    };
    const initialState =
        await ChromeHelper.getInstance().startMonitorStorage(onChange);
    if (initialState === StorageMonitorStatus.LOW) {
      this.toggleLowStorageWarning(true);
    }
    return initialState !== StorageMonitorStatus.CRITICALLY_LOW;
  }

  /**
   * Toggles pause/resume state of video recording.
   *
   * @return Promise resolved when recording is paused/resumed.
   */
  async togglePaused(): Promise<void> {
    if (!state.get(state.State.RECORDING)) {
      return;
    }
    if (this.togglePausedInternal !== null) {
      return this.togglePausedInternal;
    }
    this.everPaused = true;

    if (this.recordingType === RecordType.TIME_LAPSE) {
      return this.togglePausedTimeLapse();
    }

    const waitable = new WaitableEvent();
    this.togglePausedInternal = waitable.wait();

    assert(this.mediaRecorder !== null);
    assert(this.mediaRecorder.state !== 'inactive');
    const toBePaused = this.mediaRecorder.state !== 'paused';
    const toggledEvent = toBePaused ? 'pause' : 'resume';

    if (!toBePaused && !(await this.resumeMonitorStorage())) {
      // Keep |togglePausedInternal| non-null to prevent pause/resume while
      // stopping the recording.
      waitable.signal();
      return;
    }

    const onToggled = () => {
      assert(this.mediaRecorder !== null);
      this.mediaRecorder.removeEventListener(toggledEvent, onToggled);
      state.set(state.State.RECORDING_PAUSED, toBePaused);
      this.togglePausedInternal = null;
      waitable.signal();
    };

    this.mediaRecorder.addEventListener(toggledEvent, onToggled);
    if (toBePaused) {
      waitable.wait().then(() => this.playPauseEffect(toBePaused));
      this.recordTime.stop({pause: true});
      this.mediaRecorder.pause();
    } else {
      await this.playPauseEffect(toBePaused);
      this.recordTime.start({resume: true});
      this.mediaRecorder.resume();
    }

    return waitable.wait();
  }

  private async togglePausedTimeLapse(): Promise<void> {
    const toggleDone = new WaitableEvent();
    this.togglePausedInternal = toggleDone.wait();
    const toBePaused = !state.get(state.State.RECORDING_PAUSED);

    if (!toBePaused && !(await this.resumeMonitorStorage())) {
      toggleDone.signal();
      return;
    }

    // Pause: Pause -> Update Timer -> Sound/Button UI
    // Resume: Sound/Button UI -> Update Timer -> Resume
    if (toBePaused) {
      state.set(state.State.RECORDING_PAUSED, true);
      this.recordTime.stop({pause: true});
      await this.playPauseEffect(true);
    } else {
      await this.playPauseEffect(false);
      this.recordTime.start({resume: true});
      state.set(state.State.RECORDING_PAUSED, false);
    }

    toggleDone.signal();
    this.togglePausedInternal = null;
  }

  private async playPauseEffect(toBePaused: boolean): Promise<void> {
    state.set(state.State.RECORDING_UI_PAUSED, toBePaused);
    await sound.play(dom.get(
        toBePaused ? '#sound-rec-pause' : '#sound-rec-start',
        HTMLAudioElement));
  }

  /**
   * Resumes storage monitoring and returns if the recording can be resumed.
   */
  private async resumeMonitorStorage(): Promise<boolean> {
    const canResume = await this.startMonitorStorage();
    if (!canResume) {
      this.autoStopped = true;
      this.stop();
    }
    return canResume;
  }

  private getEncoderParameters(): h264.EncoderParameters|null {
    if (avc1Parameters !== null) {
      return avc1Parameters;
    }
    const preference = encoderPreference.get(loadTimeData.getBoard()) ??
        {profile: h264.Profile.HIGH, multiplier: 2};
    let {profile, multiplier} = preference;
    if (this.recordingType === RecordType.TIME_LAPSE) {
      multiplier = Math.max(multiplier, TIME_LAPSE_MIN_BITRATE_MULTIPLIER);
    }
    const {width, height, frameRate} =
        getVideoTrackSettings(this.getVideoTrack());
    const resolution = new Resolution(width, height);
    const bitrate = resolution.area * multiplier;
    const level = h264.getMinimalLevel(profile, bitrate, frameRate, resolution);
    if (level === null) {
      reportError(
          ErrorType.NO_AVAILABLE_LEVEL, ErrorLevel.WARNING,
          new Error(
              `No valid level found for ` +
              `profile: ${h264.getProfileName(profile)} bitrate: ${bitrate}`));
      return null;
    }
    return {profile, level, bitrate};
  }

  private getRecordingStream(): MediaStream {
    if (this.captureStream !== null) {
      return this.captureStream;
    }
    return this.video.getStream();
  }

  /**
   * Gets video track of recording stream.
   */
  private getVideoTrack(): MediaStreamVideoTrack {
    // The type annotation on MediaStream.getVideoTracks() in @types/webrtc is
    // not specific enough.
    // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
    return this.getRecordingStream().getVideoTracks()[0] as
        MediaStreamVideoTrack;
  }

  async start(): Promise<[Promise<void>]> {
    assert(this.snapshotting === null);
    this.togglePausedInternal = null;
    this.everPaused = false;
    this.autoStopped = false;
    this.stopped = false;

    this.recordingType = this.getToggledRecordOption();
    if (this.recordingType === RecordType.NORMAL ||
        this.recordingType === RecordType.TIME_LAPSE) {
      const canStart = await this.startMonitorStorage();
      if (!canStart) {
        ChromeHelper.getInstance().stopMonitorStorage();
        throw new LowStorageError();
      }
    }

    const isSoundEnded =
        await sound.play(dom.get('#sound-rec-start', HTMLAudioElement));
    if (!isSoundEnded) {
      throw new CanceledError('Recording sound is canceled');
    }

    if (this.captureStream === null) {
      if (expert.isEnabled(
              expert.ExpertOption.ENABLE_MULTISTREAM_RECORDING_CHROME)) {
        this.captureStream =
            StreamManagerChrome.getInstance().getCaptureStream();
      } else if (this.captureConstraints !== null) {
        this.captureStream =
            await StreamManager.getInstance().openCaptureStream(
                this.captureConstraints);
      }
    }
    if (this.recordingImageCapture === null) {
      this.recordingImageCapture = new CrosImageCapture(this.getVideoTrack());
    }

    let param: h264.EncoderParameters|null = null;
    try {
      param = this.getEncoderParameters();
      const mimeType = getVideoMimeType(param);
      if (!MediaRecorder.isTypeSupported(mimeType)) {
        throw new Error(
            `The preferred mimeType "${mimeType}" is not supported.`);
      }
      const option: MediaRecorderOptions = {mimeType};
      if (param !== null) {
        option.videoBitsPerSecond = param.bitrate;
      }
      this.mediaRecorder = new MediaRecorder(this.getRecordingStream(), option);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_RECORD_START_FAILED);
      throw e;
    }

    if (this.stopped) {
      throw new CanceledError('Recording stopped');
    }

    // TODO(b/191950622): Remove complex state logic bind with this enable flag
    // after GIF recording move outside of expert mode and replace it with
    // |RECORD_TYPE_GIF|.
    state.set(
        state.State.ENABLE_GIF_RECORDING,
        this.recordingType === RecordType.GIF);
    if (this.recordingType === RecordType.GIF) {
      state.set(state.State.RECORDING, true);
      this.gifRecordTime.start({resume: false});

      let gifSaver = null;
      try {
        gifSaver = await this.captureGif();
      } catch (e) {
        if (e instanceof NoFrameError) {
          toast.show(I18nString.ERROR_MSG_VIDEO_TOO_SHORT);
          return [Promise.resolve()];
        }
        throw e;
      } finally {
        state.set(state.State.RECORDING, false);
        this.gifRecordTime.stop({pause: false});
      }

      const gifName = (new Filenamer()).newVideoName(VideoType.GIF);
      // TODO(b/191950622): Close capture stream before onGifCaptureDone()
      // opening preview page when multi-stream recording enabled.
      return [this.handler.onGifCaptureDone({
        name: gifName,
        gifSaver,
        resolution: this.captureResolution,
        duration: this.gifRecordTime.inMilliseconds(),
      })];
    } else if (this.recordingType === RecordType.TIME_LAPSE) {
      // TODO(b/279865370): Don't pause when the confirm dialog is shown.
      window.addEventListener('beforeunload', beforeUnloadListener);

      this.recordTime.start({resume: false});
      let timeLapseSaver: TimeLapseSaver|null = null;
      try {
        assert(param !== null);
        timeLapseSaver = await this.captureTimeLapse(param);
      } finally {
        state.set(state.State.RECORDING, false);
        this.recordTime.stop({pause: false});
        window.removeEventListener('beforeunload', beforeUnloadListener);
      }

      if (this.recordTime.inMilliseconds() <
          (MINIMUM_VIDEO_DURATION_IN_MILLISECONDS * TIME_LAPSE_INITIAL_SPEED)) {
        toast.show(I18nString.ERROR_MSG_VIDEO_TOO_SHORT);
        timeLapseSaver.cancel();
        return [Promise.resolve()];
      }

      return [this.handler.onTimeLapseCaptureDone({
        autoStopped: this.autoStopped,
        duration: this.recordTime.inMilliseconds(),
        everPaused: this.everPaused,
        resolution: this.captureResolution,
        speed: timeLapseSaver.speed,
        timeLapseSaver,
      })];
    } else {
      this.recordTime.start({resume: false});
      let videoSaver: VideoSaver|null = null;

      const isVideoTooShort = () => this.recordTime.inMilliseconds() <
          MINIMUM_VIDEO_DURATION_IN_MILLISECONDS;

      try {
        try {
          videoSaver = await this.captureVideo();
        } finally {
          this.recordTime.stop({pause: false});
          sound.play(dom.get('#sound-rec-end', HTMLAudioElement));
          await this.snapshotting;
        }
      } catch (e) {
        // Tolerates the error if it is due to the very short duration. Reports
        // for other errors.
        if (!(e instanceof NoChunkError && isVideoTooShort())) {
          toast.show(I18nString.ERROR_MSG_EMPTY_RECORDING);
          throw e;
        }
      }

      if (isVideoTooShort()) {
        assert(videoSaver !== null);
        toast.show(I18nString.ERROR_MSG_VIDEO_TOO_SHORT);
        await videoSaver.cancel();
        return [this.snapshotting ?? Promise.resolve()];
      }

      return [(async () => {
        assert(videoSaver !== null);
        await this.handler.onVideoCaptureDone({
          autoStopped: this.autoStopped,
          resolution: this.captureResolution,
          duration: this.recordTime.inMilliseconds(),
          videoSaver,
          everPaused: this.everPaused,
        });
        await this.snapshotting;
      })()];
    }
  }

  override stop(): void {
    this.stopped = true;
    ChromeHelper.getInstance().stopMonitorStorage();
    this.toggleLowStorageWarning(false);
    if (!state.get(state.State.RECORDING)) {
      return;
    }
    if (this.recordingType === RecordType.GIF ||
        this.recordingType === RecordType.TIME_LAPSE) {
      state.set(state.State.RECORDING, false);
      state.set(state.State.RECORDING_PAUSED, false);
      state.set(state.State.RECORDING_UI_PAUSED, false);
    } else {
      sound.cancel(dom.get('#sound-rec-start', HTMLAudioElement));

      if (this.mediaRecorder !== null &&
          (this.mediaRecorder.state === 'recording' ||
           this.mediaRecorder.state === 'paused')) {
        this.mediaRecorder.stop();
        window.removeEventListener('beforeunload', beforeUnloadListener);
      }
    }
  }

  /**
   * Starts recording gif animation and waits for stop recording event triggered
   * by stop shutter or time out over 5 seconds.
   */
  private async captureGif(): Promise<GifSaver> {
    // TODO(b/191950622): Grab frames from capture stream when multistream
    // enabled.
    const video = this.video.video;
    const videoTrack = this.getVideoTrack();
    let {videoWidth: width, videoHeight: height} = video;
    if (width > GIF_MAX_SIDE || height > GIF_MAX_SIDE) {
      const ratio = GIF_MAX_SIDE / Math.max(width, height);
      width = Math.round(width * ratio);
      height = Math.round(height * ratio);
    }
    const gifSaver = await GifSaver.create(new Resolution(width, height));
    const canvas = new OffscreenCanvas(width, height);
    const context = assertInstanceof(
        canvas.getContext('2d', {willReadFrequently: true}),
        OffscreenCanvasRenderingContext2D);
    if (videoTrack.readyState === 'ended') {
      throw new NoFrameError();
    }
    const frames = await new Promise<number>((resolve) => {
      let encodedFrames = 0;
      let writtenFrames = 0;
      let handle = 0;
      function stopRecording() {
        video.cancelVideoFrameCallback(handle);
        videoTrack.removeEventListener('ended', stopRecording);
        resolve(writtenFrames);
      }
      function updateCanvas() {
        if (!state.get(state.State.RECORDING)) {
          stopRecording();
          return;
        }
        encodedFrames++;
        if (encodedFrames % GRAB_GIF_FRAME_RATIO === 0) {
          writtenFrames++;
          context.drawImage(video, 0, 0, width, height);
          gifSaver.write(context.getImageData(0, 0, width, height).data);
        }
        handle = video.requestVideoFrameCallback(updateCanvas);
      }
      videoTrack.addEventListener('ended', stopRecording);
      handle = video.requestVideoFrameCallback(updateCanvas);
    });
    if (frames === 0) {
      throw new NoFrameError();
    }
    return gifSaver;
  }

  /**
   * Initial time-lapse saver with specified encoder parameters. Then, Starts
   * recording time-lapse and waits for stop recording event.
   */
  private async captureTimeLapse(param: h264.EncoderParameters):
      Promise<TimeLapseSaver> {
    const encoderConfig = getVideoEncoderConfig(param, this.captureResolution);

    // Creates a saver given the initial speed.
    const saver = await TimeLapseSaver.create(
        encoderConfig, this.captureResolution, this.frameRate,
        TIME_LAPSE_INITIAL_SPEED);

    // Creates a frame reader from track processor.
    const track = this.getVideoTrack();
    const trackProcessor = new MediaStreamTrackProcessor({track});
    const reader = trackProcessor.readable.getReader();

    state.set(state.State.RECORDING, true);
    const frames = await new Promise<number>((resolve, reject) => {
      let frameCount = 0;
      let writtenFrameCount = 0;

      async function onError(error: unknown) {
        await saver.cancel();
        reject(error);
      }
      saver.setErrorCallback(onError);

      async function updateFrame(): Promise<void> {
        if (!state.get(state.State.RECORDING)) {
          resolve(writtenFrameCount);
          return;
        }
        if (state.get(state.State.RECORDING_PAUSED)) {
          state.addOneTimeObserver(state.State.RECORDING_PAUSED, updateFrame);
          return;
        }
        let frame: VideoFrame|null = null;
        try {
          const {done, value} = await reader.read();
          if (done) {
            resolve(writtenFrameCount);
            return;
          }
          frame = value;
          if (frameCount % saver.speed === 0) {
            saver.write(frame, frameCount);
            writtenFrameCount++;
          }
        } catch (e) {
          onError(e);
        } finally {
          if (frame !== null) {
            frame.close();
          }
        }
        frameCount++;
        updateFrame();
      }
      updateFrame();
    });

    if (frames === 0) {
      throw new NoFrameError();
    }

    return saver;
  }

  /**
   * Starts recording and waits for stop recording event triggered by stop
   * shutter.
   */
  private async captureVideo(): Promise<VideoSaver> {
    const saver = await this.handler.createVideoSaver();

    try {
      await new Promise((resolve, reject) => {
        let noChunk = true;

        function onDataAvailable(event: BlobEvent) {
          if (event.data.size > 0) {
            noChunk = false;
            saver.write(event.data);
          }
        }

        const onStop = async () => {
          assert(this.mediaRecorder !== null);

          state.set(state.State.RECORDING, false);
          state.set(state.State.RECORDING_PAUSED, false);
          state.set(state.State.RECORDING_UI_PAUSED, false);

          this.mediaRecorder.removeEventListener(
              'dataavailable', onDataAvailable);
          this.mediaRecorder.removeEventListener('stop', onStop);

          if (noChunk) {
            reject(new NoChunkError());
          } else {
            resolve(saver);
          }
        };

        const onStart = () => {
          assert(this.mediaRecorder !== null);

          if (this.stopped) {
            this.mediaRecorder.stop();
            window.removeEventListener('beforeunload', beforeUnloadListener);
            return;
          }
          state.set(state.State.RECORDING, true);
        };

        assert(this.mediaRecorder !== null);
        this.mediaRecorder.addEventListener('dataavailable', onDataAvailable);
        this.mediaRecorder.addEventListener('stop', onStop);
        this.mediaRecorder.addEventListener('start', onStart, {once: true});

        window.addEventListener('beforeunload', beforeUnloadListener);

        this.mediaRecorder.start(100);
        state.set(state.State.RECORDING_PAUSED, false);
        state.set(state.State.RECORDING_UI_PAUSED, false);
      });
      return saver;
    } catch (e) {
      await saver.cancel();
      throw e;
    }
  }
}

/**
 * Factory for creating video mode capture object.
 */
export class VideoFactory extends ModeFactory {
  /**
   * @param constraints Constraints for preview
   *     stream.
   */
  constructor(
      constraints: StreamConstraints,
      captureResolution: Resolution|null,
      private readonly snapshotResolution: Resolution|null,
      private readonly handler: VideoHandler,
  ) {
    super(constraints, captureResolution);
  }

  produce(): ModeBase {
    let captureConstraints = null;
    if (expert.isEnabled(expert.ExpertOption.ENABLE_MULTISTREAM_RECORDING)) {
      const {width, height} = assertExists(this.captureResolution);
      captureConstraints = {
        deviceId: this.constraints.deviceId,
        audio: this.constraints.audio,
        video: {
          frameRate: this.constraints.video.frameRate,
          width,
          height,
        },
      };
    }
    assert(this.previewVideo !== null);
    assert(this.facing !== null);
    const frameRate =
        getFpsRangeFromConstraints(this.constraints.video.frameRate).minFps;
    return new Video(
        this.previewVideo, captureConstraints, this.captureResolution,
        this.snapshotResolution, this.facing, this.handler, frameRate);
  }
}
