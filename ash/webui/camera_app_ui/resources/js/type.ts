// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists, assertInstanceof} from './assert.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * Photo or video resolution.
 */
export class Resolution {
  readonly width: number;

  readonly height: number;

  constructor();
  constructor(width: number, height: number);
  constructor(width?: number, height?: number) {
    this.width = width ?? 0;
    this.height = height ?? -1;
  }

  /**
   * @return Total pixel number.
   */
  get area(): number {
    return this.width * this.height;
  }

  /**
   * Aspect ratio calculates from width divided by height.
   */
  get aspectRatio(): number {
    // Special aspect ratio mapping rule, see http://b/147986763.
    if (this.width === 848 && this.height === 480) {
      return (new Resolution(16, 9)).aspectRatio;
    }
    // Approximate to 4 decimal places to prevent precision error during
    // comparing.
    return parseFloat((this.width / this.height).toFixed(4));
  }

  /**
   * @return The amount of mega pixels to 1 decimal place.
   */
  get mp(): number {
    return parseFloat((this.area / 1000000).toFixed(1));
  }

  /**
   * Compares width/height of resolutions, see if they are equal or not.
   *
   * @param resolution Resolution to be compared with.
   */
  equals(resolution: Resolution|null): boolean {
    if (resolution === null) {
      return false;
    }
    return this.width === resolution.width && this.height === resolution.height;
  }

  /**
   * Compares width/height of resolutions, see if they are equal or not. It also
   * returns true if the resolution is rotated.
   *
   * @param resolution Resolution to be compared with.
   */
  equalsWithRotation(resolution: Resolution): boolean {
    return (this.width === resolution.width &&
            this.height === resolution.height) ||
        (this.width === resolution.height && this.height === resolution.width);
  }

  /**
   * Compares aspect ratio of resolutions, see if they are equal or not.
   *
   * @param resolution Resolution to be compared with.
   */
  aspectRatioEquals(resolution: Resolution): boolean {
    return this.aspectRatio === resolution.aspectRatio;
  }

  /**
   * Create Resolution object from string.
   */
  static fromString(s: string): Resolution {
    const [width, height] = s.split('x').map((x) => Number(x));
    return new Resolution(width, height);
  }

  toString(): string {
    return `${this.width}x${this.height}`;
  }
}

/**
 * Types of common mime types.
 */
export enum MimeType {
  GIF = 'image/gif',
  JPEG = 'image/jpeg',
  JSON = 'application/json',
  MP4 = 'video/mp4',
  PDF = 'application/pdf',
}

/**
 * Capture modes.
 */
export enum Mode {
  PHOTO = 'photo',
  PORTRAIT = 'portrait',
  SCAN = 'scan',
  VIDEO = 'video',
}

/**
 * Camera facings.
 */
export enum Facing {
  ENVIRONMENT = 'environment',
  EXTERNAL = 'external',
  USER = 'user',
  // VIRTUAL_{facing} is for labeling video device for configuring extra stream
  // from corresponding {facing} video device.
  VIRTUAL_ENV = 'virtual_environment',
  VIRTUAL_EXT = 'virtual_external',
  VIRTUAL_USER = 'virtual_user',
}

export enum ViewName {
  CAMERA = 'view-camera',
  DOCUMENT_REVIEW = 'view-document-review',
  EXPERT_SETTINGS = 'view-expert-settings',
  FLASH = 'view-flash',
  LOW_STORAGE_DIALOG = 'view-low-storage-dialog',
  OPTION_PANEL = 'view-option-panel',
  PHOTO_ASPECT_RATIO_SETTINGS = 'view-photo-aspect-ratio-settings',
  PHOTO_RESOLUTION_SETTINGS = 'view-photo-resolution-settings',
  PTZ_PANEL = 'view-ptz-panel',
  REVIEW = 'view-review',
  SETTINGS = 'view-settings',
  SPLASH = 'view-splash',
  SUPER_RES_INTRO_DIALOG = 'view-super-res-intro-dialog',
  VIDEO_RESOLUTION_SETTINGS = 'view-video-resolution-settings',
  WARNING = 'view-warning',
}

export enum VideoType {
  GIF = 'gif',
  MP4 = 'mp4',
}

export enum PhotoResolutionLevel {
  FULL = 'full',
  MEDIUM = 'medium',
  UNKNOWN = 'unknown',
}

/* eslint-disable cca/string-enum-order */
export enum VideoResolutionLevel {
  FOUR_K = '4K',
  QUAD_HD = 'Quad HD',
  FULL_HD = 'Full HD',
  HD = 'HD',
  THREE_SIXTY_P = '360p',

  FULL = 'full',
  MEDIUM = 'medium',

  UNKNOWN = 'unknown',
}
/* eslint-enable cca/string-enum-order */

export enum AspectRatioSet {
  RATIO_4_3 = 1.3333,
  RATIO_16_9 = 1.7778,
  RATIO_OTHER = 0.0000,
  RATIO_SQUARE = 1.0000,
}

export enum Rotation {
  ANGLE_0 = 0,
  ANGLE_90 = 90,
  ANGLE_180 = 180,
  ANGLE_270 = 270,
}
// `ROTATION_ORDER` is used for document scanning fix mode to show/crop images.
// The length must be fixed at 4.
export const ROTATION_ORDER =
    Object.values(Rotation).filter((r): r is Rotation => typeof r === 'number');

export interface VideoConfig {
  width: number;
  height: number;
  maxFps: number;
}

export interface FpsRange {
  minFps: number;
  maxFps: number;
}

/**
 * A list of resolutions.
 */
export type ResolutionList = Resolution[];

/**
 * Map of all available resolution to its maximal supported capture fps. The key
 * of the map is the resolution and the corresponding value is the maximal
 * capture fps under that resolution.
 */
export type MaxFpsInfo = Record<string, number>;

/**
 * List of supported capture fps ranges.
 */
export type FpsRangeList = FpsRange[];

/**
 * Type for performance event.
 */
export enum PerfEvent {
  // In all modes, the duration between the camera switch button being clicked
  // and the preview stream being updated.
  CAMERA_SWITCHING = 'camera-switching',
  // In Doc Scan mode, the duration between a shutter sound playing and the
  // image appearing in the review page.
  DOCUMENT_CAPTURE_POST_PROCESSING = 'document-capture-post-processing',
  // In Doc Scan mode, the duration between "Save as PDF" button being clicked
  // and the review page closing.
  DOCUMENT_PDF_SAVING = 'document-pdf-saving',
  // In GIF mode, the duration between GIF recording stopping and the temporal
  // GIF appearing in the review page.
  GIF_CAPTURE_POST_PROCESSING = 'gif-capture-post-processing',
  // In GIF mode, the duration between "Save" button being clicked and the
  // result file saving finished.
  GIF_CAPTURE_SAVING = 'gif-capture-saving',
  // Used for testing. The duration between app window being created and the app
  // being launched.
  LAUNCHING_FROM_LAUNCH_APP_COLD = 'launching-from-launch-app-cold',
  // Used for testing. The duration between app window being created and the app
  // being launched.
  LAUNCHING_FROM_LAUNCH_APP_WARM = 'launching-from-launch-app-warm',
  // The duration between CCA window being created and the preview stream
  // appearing.
  LAUNCHING_FROM_WINDOW_CREATION = 'launching-from-window-creation',
  // In all modes, the duration between the mode switch button being clicked and
  // the preview stream being updated.
  MODE_SWITCHING = 'mode-switching',
  // In Photo mode, the duration between a snapshot of the preview being scanned
  // by OCR(automatically, with 500ms intervals) and the scanned result
  // appearing in the preview. The result might not be shown if it is empty or
  // if other scanners have detected results.
  OCR_SCANNING = 'ocr-scanning',
  // In Photo mode, the duration between a shutter sound playing and the
  // result file saving finished.
  PHOTO_CAPTURE_POST_PROCESSING_SAVING = 'photo-capture-post-processing-saving',
  // In Photo, Doc Scan and Portrait mode, the duration between the shutter
  // button being clicked or a timer expiring and a shutter sound playing.
  PHOTO_CAPTURE_SHUTTER = 'photo-capture-shutter',
  // In Portrait mode, the duration between a shutter sound playing and the
  // two result files saving finished.
  PORTRAIT_MODE_CAPTURE_POST_PROCESSING_SAVING =
      'portrait-mode-capture-post-processing-saving',
  // In Video mode, the duration between the video snapshot button being clicked
  // and the result file saving finished.
  SNAPSHOT_TAKING = 'snapshot-taking',
  // In Time lapse mode, the duration between a shutter sound playing and
  // the result file saving finished.
  TIME_LAPSE_CAPTURE_POST_PROCESSING_SAVING =
      'time-lapse-capture-post-processing-saving',
  // In Video mode, the duration between the shutter button being clicked to
  // stop recording and the result file saving finished.
  VIDEO_CAPTURE_POST_PROCESSING_SAVING = 'video-capture-post-processing-saving',
}

export enum Pressure {
  NOMINAL,
  FAIR,
  SERIOUS,
  CRITICAL,
}

export interface ImageBlob {
  blob: Blob;
  resolution: Resolution;
}

// The key-value pair of the entries in metadata are stored as key-value of an
// |Object| type
export type Metadata = Record<string, unknown>;

export interface PerfInformation {
  hasError?: boolean;
  resolution?: Resolution;
  facing?: Facing;
  pageCount?: number;  // Only for DOCUMENT_PDF_SAVING
  pressure?: Pressure;
}

export interface PerfEntry {
  event: PerfEvent;
  duration: number;
  perfInfo: PerfInformation;
}

export interface VideoTrackSettings {
  deviceId: string;
  width: number;
  height: number;
  frameRate: number;
}

/**
 * Gets video track settings from a video track.
 *
 * This asserts that all property that should exists on video track settings
 * (.width, .height, .deviceId, .frameRate) all exists and narrow the type.
 */
export function getVideoTrackSettings(videoTrack: MediaStreamTrack):
    VideoTrackSettings {
  // TODO(pihsun): The type from TypeScript lib.dom.d.ts is wrong on Chrome and
  // the .deviceId should never be undefined. Try to override that when we have
  // newer TypeScript compiler (>= 4.5) that supports overriding lib.dom.d.ts.
  const {deviceId, width, height, frameRate} = videoTrack.getSettings();
  return {
    deviceId: assertExists(deviceId),
    width: assertExists(width),
    height: assertExists(height),
    frameRate: assertExists(frameRate),
  };
}

/**
 * A proxy to get preview video or stream with notification of when the video
 * stream is expired.
 */
export class PreviewVideo {
  constructor(
      readonly video: HTMLVideoElement, readonly onExpired: WaitableEvent) {}

  getStream(): MediaStream {
    return assertInstanceof(this.video.srcObject, MediaStream);
  }

  getVideoTrack(): MediaStreamTrack {
    return this.getStream().getVideoTracks()[0];
  }

  getVideoSettings(): VideoTrackSettings {
    return getVideoTrackSettings(this.getVideoTrack());
  }

  isExpired(): boolean {
    return this.onExpired.isSignaled();
  }
}

/**
 * Error reported in testing run.
 */
export interface ErrorInfo {
  type: ErrorType;
  level: ErrorLevel;
  stack: string;
  time: number;
  name: string;
}

/**
 * Types of error used in ERROR metrics.
 */
export enum ErrorType {
  BIG_BUFFER_FAILURE = 'big-buffer-failure',
  BROKEN_THUMBNAIL = 'broken-thumbnail',
  CHECK_COVER_FAILURE = 'check-cover-failed',
  DEVICE_INFO_UPDATE_FAILURE = 'device-info-update-failure',
  DEVICE_NOT_EXIST = 'device-not-exist',
  EMPTY_FILE = 'empty-file',
  FILE_SYSTEM_FAILURE = 'file-system-failure',
  FRAME_ROTATION_NOT_DISABLED = 'frame-rotation-not-disabled',
  HANDLE_CAMERA_RESULT_FAILURE = 'handle-camera-result-failure',
  INVALID_REVIEW_UI_STATE = 'invalid-review-ui-state',
  METADATA_MAPPING_FAILURE = 'metadata-mapping-failure',
  MULTI_WINDOW_HANDLING_FAILURE = 'multi-window-handling-failure',
  MULTIPLE_STREAMS_FAILURE = 'multiple-streams-failure',
  NO_AVAILABLE_LEVEL = 'no-available-level',
  PERF_METRICS_FAILURE = 'perf-metrics-failure',
  PRELOAD_IMAGE_FAILURE = 'preload-image-failure',
  RESUME_CAMERA_FAILURE = 'resume-camera-failure',
  RESUME_PAUSE_FAILURE = 'resume-pause-failure',
  SET_FPS_RANGE_FAILURE = 'set-fps-range-failure',
  START_CAMERA_FAILURE = 'start-camera-failure',
  START_CAPTURE_FAILURE = 'start-capture-failure',
  STOP_CAPTURE_FAILURE = 'stop-capture-failure',
  SUSPEND_CAMERA_FAILURE = 'suspend-camera-failure',
  UNCAUGHT_ERROR = 'uncaught-error',
  UNCAUGHT_PROMISE = 'uncaught-promise',
  UNSAFE_INTEGER = 'unsafe-integer',
  UNSUPPORTED_PROTOCOL = 'unsupported-protocol',
}

/**
 * Error level used in ERROR metrics.
 */
export enum ErrorLevel {
  ERROR = 'ERROR',
  WARNING = 'WARNING',
}

/**
 * Throws when a method is not implemented.
 */
export class NotImplementedError extends Error {
  constructor(message = 'Method is not implemented') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when an action is canceled.
 */
export class CanceledError extends Error {
  constructor(message = 'The action is canceled') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when an element fails to load a source.
 */
export class LoadError extends Error {
  constructor(message = 'Source failed to load') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when an media element fails to play.
 */
export class PlayError extends Error {
  constructor(message = 'Media element failed to play') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when an media element play a malformed file.
 */
export class PlayMalformedError extends Error {
  constructor(message = 'Media element failed to play a malformed file') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when the data to generate thumbnail is totally empty.
 */
export class EmptyThumbnailError extends Error {
  constructor(message = 'The thumbnail is empty') {
    super(message);
    this.name = this.constructor.name;
  }
}

export class LowStorageError extends Error {
  constructor() {
    const message = 'Cannot start recording due to low storage.';
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when the recording is ended with no chunk returned.
 */
export class NoChunkError extends Error {
  constructor(message = 'No chunk is received during recording session') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when the GIF or time lapse recording is ended with no frame captured.
 */
export class NoFrameError extends Error {
  constructor(message = 'No frames captured during the recording') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when the portrait mode fails to detect a human face.
 */
export class PortraitErrorNoFaceDetected extends Error {
  constructor(message = 'No human face detected in the scene') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Throws when the camera is suspended while camera effects are ongoing.
 */
export class CameraSuspendError extends Error {
  constructor(message = 'Camera suspended') {
    super(message);
    this.name = this.constructor.name;
  }
}

export class NoCameraError extends Error {
  constructor(message = 'No available cameras') {
    super(message);
    this.name = this.constructor.name;
  }
}

/**
 * Types of local storage key.
 */
export enum LocalStorageKey {
  CUSTOM_VIDEO_PARAMETERS = 'customVideoParameters',
  ENABLE_FPS_PICKER = 'enableFPSPicker',
  ENABLE_FULL_SIZED_VIDEO_SNAPSHOT = 'enableFullSizedVideoSnapshot',
  ENABLE_PREVIEW_OCR = 'enablePreviewOcr',
  ENABLE_PTZ_FOR_BUILTIN = 'enablePTZForBuiltin',
  EXPERT_MODE = 'expert',
  FIRST_OPENING = 'firstOpening',
  GA_ID_REFRESH_TIME = 'gaIdRefreshTime',
  GA_USER_ID = 'google-analytics.analytics.user-id',
  GA4_CLIENT_ID = 'ga4ClientId',
  MIRRORING_TOGGLES = 'mirroringToggles',
  PREF_DEVICE_PHOTO_ASPECT_RATIO_SET = 'devicePhotoAspectRatioSet',
  PREF_DEVICE_PHOTO_RESOLUTION_EXPERT = 'devicePhotoResolutionExpert',
  PREF_DEVICE_PHOTO_RESOLUTION_LEVEL = 'devicePhotoResolutionLevel',
  PREF_DEVICE_VIDEO_RESOLUTION_EXPERT = 'deviceVideoResolutionExpert',
  PREF_DEVICE_VIDEO_RESOLUTION_FPS = 'deviceVideoResolutionFps',
  PREF_DEVICE_VIDEO_RESOLUTION_LEVEL = 'deviceVideoResolutionLevel',
  PREVIEW_OCR_TOAST_SHOWN = 'previewOcrToastShown',
  PRINT_PERFORMANCE_LOGS = 'printPerformanceLogs',
  SAVE_METADATA = 'saveMetadata',
  SHOW_ALL_RESOLUTIONS = 'showAllResolutions',
  SHOW_METADATA = 'showMetadata',
  SUPER_RES_DIALOG_SHOWN = 'superResDialogShown',
  TOGGLE_MIC = 'toggleMic',
}

/**
 * Type of low storage dialog.
 */
export enum LowStorageDialogType {
  AUTO_STOP = 'auto-stop',
  CANNOT_START = 'cannot-start',
}

/**
 * A rectangle representing a crop region with size (width, height) and having
 * the top-left coordinate at (x, y).
 */
export interface CropRegionRect {
  height: number;
  width: number;
  x: number;
  y: number;
}

export type Awaitable<T> = PromiseLike<T>|T;
