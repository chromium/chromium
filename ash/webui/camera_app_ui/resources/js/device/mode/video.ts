// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertInstanceof,
} from '../../assert.js';
import {AsyncJobQueue} from '../../async_job_queue.js';
import * as dom from '../../dom.js';
import {reportError} from '../../error.js';
import * as h264 from '../../h264.js';
import {I18nString} from '../../i18n_string.js';
import {LowStorageActionType, sendLowStorageEvent} from '../../metrics.js';
import {Filenamer} from '../../models/file_namer.js';
import * as loadTimeData from '../../models/load_time_data.js';
import {
  GifSaver,
  TimeLapseEncoderArgs,
  TimeLapseSaver,
  VideoSaver,
} from '../../models/video_saver.js';
import {ChromeHelper} from '../../mojo/chrome_helper.js';
import {DeviceOperator} from '../../mojo/device_operator.js';
import {CrosImageCapture} from '../../mojo/image_capture.js';
import {StorageMonitorStatus} from '../../mojo/type.js';
import {PerfLogger} from '../../perf.js';
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
  PerfEvent,
  PreviewVideo,
  Resolution,
  VideoType,
} from '../../type.js';
import {getFpsRangeFromConstraints, sleep} from '../../util.js';
import {WaitableEvent} from '../../waitable_event.js';
import {StreamConstraints} from '../stream_constraints.js';

import {ModeBase, ModeFactory} from './mode_base.js';
import {PhotoResult} from './photo.js';
import {RecordTime} from './record_time.js';

/**
 * Maps from board name to its default encoding profile and bitrate multiplier.
 */
const encoderPreference = new Map([
  ['brya', {profile: h264.Profile.HIGH, multiplier: 8}],
  ['corsola', {profile: h264.Profile.HIGH, multiplier: 6}],
  ['dedede', {profile: h264.Profile.HIGH, multiplier: 8}],
  ['geralt', {profile: h264.Profile.HIGH, multiplier: 8}],
  ['strongbad', {profile: h264.Profile.HIGH, multiplier: 6}],
  ['trogdor', {profile: h264.Profile.HIGH, multiplier: 6}],
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
   * Creates TimeLapseSaver to save time-lapse capture result.
   */
  createTimeLapseSaver(encoderArgs: TimeLapseEncoderArgs, speed: number):
      Promise<TimeLapseSaver>;

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
  private readonly recordTime = new RecordTime(() => this.stop());

  /**
   * Record type of ongoing recording.
   */
  private recordingType: RecordType = RecordType.NORMAL;

  /**
   * Ongoing video snapshot queue.
   */
  private readonly snapshottingQueue = new AsyncJobQueue('drop');

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
   * Callback to stop the loop of requesting video frames in the gif mode.
   */
  private stopCapturingGifCallback: (() => void)|null = null;

  /**
   * HTMLElement displaying warning about low storage.
   */
  private readonly lowStorageWarningNudge = dom.get('#nudge', HTMLDivElement);

  constructor(
      video: PreviewVideo,
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
  }

  override async clear(): Promise<void> {
    await this.stopCapture();
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
   */
  takeSnapshot(): void {
    this.snapshottingQueue.push(async () => {
      if (!state.get(state.State.RECORDING)) {
        return;
      }
      state.set(state.State.SNAPSHOTTING, true);
      const perfLogger = PerfLogger.getInstance();
      perfLogger.start(PerfEvent.SNAPSHOT_TAKING);
      let hasError = true;
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
        hasError = false;
      } finally {
        perfLogger.stop(PerfEvent.SNAPSHOT_TAKING, {
          resolution: this.captureResolution,
          facing: this.facing,
          hasError,
        });
        state.set(state.State.SNAPSHOTTING, false);
      }
    });
  }

  private toggleLowStorageWarning(show: boolean): void {
    if (show) {
      sendLowStorageEvent(LowStorageActionType.SHOW_WARNING_MSG);
    }
    this.lowStorageWarningNudge.hidden = !show;
  }

  /**
   * Starts monitor storage status and returns initial status.
   *
   * @return Promise resolved to boolean indicating whether users can
   * start/resume the recording.
   */
  private async startMonitorStorage(): Promise<boolean> {
    const onChange = (newState: StorageMonitorStatus) => {
      if (newState === StorageMonitorStatus.kNormal) {
        this.toggleLowStorageWarning(false);
      } else if (newState === StorageMonitorStatus.kLow) {
        this.toggleLowStorageWarning(true);
      } else if (newState === StorageMonitorStatus.kCriticallyLow) {
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
    if (initialState === StorageMonitorStatus.kLow) {
      this.toggleLowStorageWarning(true);
    }
    return initialState !== StorageMonitorStatus.kCriticallyLow;
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
    this.everPaused = true;

    if (this.recordingType === RecordType.TIME_LAPSE) {
      return this.togglePausedTimeLapse();
    }

    assert(this.mediaRecorder !== null);
    assert(this.mediaRecorder.state !== 'inactive');
    const toBePaused = this.mediaRecorder.state !== 'paused';
    const toggledEvent = toBePaused ? 'pause' : 'resume';

    if (!toBePaused && !(await this.resumeMonitorStorage())) {
      return;
    }

    const waitable = new WaitableEvent();
    function onToggled() {
      state.set(state.State.RECORDING_PAUSED, toBePaused);
      waitable.signal();
    }

    this.mediaRecorder.addEventListener(toggledEvent, onToggled, {once: true});
    // Pause: Pause Timer & Recorder -> Wait Recorder pause -> Sound/Button UI
    // Resume: Sound/Button UI -> Update Timer -> Resume Recorder
    if (toBePaused) {
      this.recordTime.pause();
      this.mediaRecorder.pause();
      await waitable.wait();
      await this.playPauseEffect(true);
    } else {
      await this.playPauseEffect(false);
      this.recordTime.resume();
      this.mediaRecorder.resume();
      await waitable.wait();
    }
  }

  private async togglePausedTimeLapse(): Promise<void> {
    const toBePaused = !state.get(state.State.RECORDING_PAUSED);

    if (!toBePaused && !(await this.resumeMonitorStorage())) {
      return;
    }

    // Pause: Pause -> Update Timer -> Sound/Button UI
    // Resume: Sound/Button UI -> Update Timer -> Resume
    if (toBePaused) {
      state.set(state.State.RECORDING_PAUSED, true);
      this.recordTime.pause();
      await this.playPauseEffect(true);
    } else {
      await this.playPauseEffect(false);
      this.recordTime.resume();
      state.set(state.State.RECORDING_PAUSED, false);
    }
  }

  private async playPauseEffect(toBePaused: boolean): Promise<void> {
    state.set(state.State.RECORDING_UI_PAUSED, toBePaused);
    await sound.play(toBePaused ? 'recordPause' : 'recordStart').result;
    // TODO(b/223338160): A temporary workaround to avoid shutter sound being
    // recorded.
    await sleep(200);
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
    const {profile} = preference;
    let {multiplier} = preference;
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

    await sound.play('recordStart').result;
    // TODO(b/223338160): A temporary workaround to avoid shutter sound being
    // recorded.
    await sleep(200);
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
      this.recordTime.start(MAX_GIF_DURATION_MS);

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
        this.recordTime.stop();
      }

      const gifName = (new Filenamer()).newVideoName(VideoType.GIF);
      return [this.handler.onGifCaptureDone({
        name: gifName,
        gifSaver,
        resolution: this.captureResolution,
        duration: this.recordTime.inMilliseconds(),
      })];
    } else if (this.recordingType === RecordType.TIME_LAPSE) {
      this.recordingImageCapture = new CrosImageCapture(this.getVideoTrack());
      const param = this.getEncoderParameters();

      // TODO(b/279865370): Don't pause when the confirm dialog is shown.
      window.addEventListener('beforeunload', beforeUnloadListener);

      this.recordTime.start();
      let timeLapseSaver: TimeLapseSaver|null = null;
      try {
        assert(param !== null);
        timeLapseSaver = await this.captureTimeLapse(param);
      } finally {
        state.set(state.State.RECORDING, false);
        this.recordTime.stop();
        window.removeEventListener('beforeunload', beforeUnloadListener);
      }

      if (this.recordTime.inMilliseconds() <
          (MINIMUM_VIDEO_DURATION_IN_MILLISECONDS * TIME_LAPSE_INITIAL_SPEED)) {
        toast.show(I18nString.ERROR_MSG_VIDEO_TOO_SHORT);
        await timeLapseSaver.cancel();
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
      this.recordingImageCapture = new CrosImageCapture(this.getVideoTrack());
      try {
        const param = this.getEncoderParameters();
        const mimeType = getVideoMimeType(param);
        if (!MediaRecorder.isTypeSupported(mimeType)) {
          throw new Error(
              `The preferred mimeType "${mimeType}" is not supported.`);
        }
        const option: MediaRecorderOptions = {mimeType};
        if (param !== null) {
          option.videoBitsPerSecond = param.bitrate;
        }
        this.mediaRecorder =
            new MediaRecorder(this.getRecordingStream(), option);
      } catch (e) {
        toast.show(I18nString.ERROR_MSG_RECORD_START_FAILED);
        throw e;
      }
      this.recordTime.start();
      let videoSaver: VideoSaver|null = null;

      const isVideoTooShort = () => this.recordTime.inMilliseconds() <
          MINIMUM_VIDEO_DURATION_IN_MILLISECONDS;

      try {
        try {
          videoSaver = await this.captureVideo();
        } finally {
          this.recordTime.stop();
          this.mediaRecorder = null;
          sound.play('recordEnd');
          await this.snapshottingQueue.flush();
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
        return [Promise.resolve()];
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
      if (this.recordingType === RecordType.GIF) {
        this.stopCapturingGifCallback?.();
      }
    } else {
      sound.cancel('recordStart');

      if (this.mediaRecorder !== null &&
          (this.mediaRecorder.state === 'recording' ||
           this.mediaRecorder.state === 'paused')) {
        this.mediaRecorder.stop();
        window.removeEventListener('beforeunload', beforeUnloadListener);
      }
    }
    this.recordingImageCapture = null;
  }

  /**
   * Starts recording gif animation and waits for stop recording event triggered
   * by stop shutter or time out over 5 seconds.
   */
  private async captureGif(): Promise<GifSaver> {
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
    if (videoTrack.readyState === 'ended' ||
        !state.get(state.State.RECORDING)) {
      throw new NoFrameError();
    }
    const frames = await new Promise<number>((resolve) => {
      let encodedFrames = 0;
      let writtenFrames = 0;
      let handle: number;
      const stopRecording = () => {
        this.stopCapturingGifCallback = null;
        video.cancelVideoFrameCallback(handle);
        videoTrack.removeEventListener('ended', stopRecording);
        resolve(writtenFrames);
      };
      function updateCanvas() {
        encodedFrames++;
        if (encodedFrames % GRAB_GIF_FRAME_RATIO === 0) {
          writtenFrames++;
          context.drawImage(video, 0, 0, width, height);
          gifSaver.write(context.getImageData(0, 0, width, height).data);
        }
        handle = video.requestVideoFrameCallback(updateCanvas);
      }
      videoTrack.addEventListener('ended', stopRecording);
      this.stopCapturingGifCallback = stopRecording;
      handle = video.requestVideoFrameCallback(updateCanvas);
    });
    if (frames === 0) {
      throw new NoFrameError();
    }
    return gifSaver;
  }

  /**
   * Creates time-lapse saver with specified encoder parameters. Then, Starts
   * recording time-lapse and waits for stop recording event.
   */
  private async captureTimeLapse(param: h264.EncoderParameters):
      Promise<TimeLapseSaver> {
    const {width, height} = getVideoTrackSettings(this.getVideoTrack());
    const resolution = new Resolution(width, height);
    const encoderConfig = getVideoEncoderConfig(param, resolution);

    // Creates a saver given the initial speed.
    const saver = await this.handler.createTimeLapseSaver(
        {
          encoderConfig,
          fps: this.frameRate,
          resolution,
        },
        TIME_LAPSE_INITIAL_SPEED);

    // Creates a frame reader from track processor.
    const track = this.getVideoTrack();
    const trackProcessor = new MediaStreamTrackProcessor({track});
    const reader = trackProcessor.readable.getReader();

    state.set(state.State.RECORDING, true);

    const errorPromise = new Promise<never>((_, reject) => {
      saver.setErrorCallback(reject);
    });

    let frameCount = 0;
    let writtenFrameCount = 0;

    while (state.get(state.State.RECORDING)) {
      if (state.get(state.State.RECORDING_PAUSED)) {
        const waitUnpaused = new WaitableEvent();
        state.addOneTimeObserver(
            state.State.RECORDING_PAUSED, () => waitUnpaused.signal());
        await Promise.race([waitUnpaused.wait(), errorPromise]);
        continue;
      }
      let frame: VideoFrame|null = null;
      try {
        const {done, value} = await Promise.race([reader.read(), errorPromise]);
        if (done) {
          break;
        }
        frame = value;
        if (frameCount % saver.speed === 0) {
          saver.write(frame, frameCount);
          writtenFrameCount++;
        }
      } catch (e) {
        await saver.cancel();
        throw e;
      } finally {
        if (frame !== null) {
          frame.close();
        }
      }
      frameCount++;
    }
    if (writtenFrameCount === 0) {
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

        async function onDataAvailable(event: BlobEvent) {
          if (event.data.size > 0) {
            noChunk = false;
            await saver.write(event.data);
          }
        }

        const onStop = () => {
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
    assert(this.previewVideo !== null);
    assert(this.facing !== null);
    const frameRate =
        getFpsRangeFromConstraints(this.constraints.video.frameRate).minFps;
    return new Video(
        this.previewVideo, this.captureResolution, this.snapshotResolution,
        this.facing, this.handler, frameRate);
  }
}
