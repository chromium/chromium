// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists, assertInstanceof} from './assert.js';

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
    // Approximate to 4 decimal places to prevent precision error during
    // comparing.
    return parseFloat((this.width / this.height).toFixed(4));
  }

  /**
   * Compares width/height of resolutions, see if they are equal or not.
   *
   * @param resolution Resolution to be compared with.
   * @return Whether width/height of resolutions are equal.
   */
  equals(resolution: Resolution): boolean {
    return this.width === resolution.width && this.height === resolution.height;
  }

  /**
   * Compares aspect ratio of resolutions, see if they are equal or not.
   *
   * @param resolution Resolution to be compared with.
   * @return Whether aspect ratio of resolutions are equal.
   */
  aspectRatioEquals(resolution: Resolution): boolean {
    return this.width * resolution.height === this.height * resolution.width;
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
  VIDEO = 'video',
  SQUARE = 'square',
  PORTRAIT = 'portrait',
  SCAN = 'scan',
}

/**
 * Camera facings.
 */
export enum Facing {
  USER = 'user',
  ENVIRONMENT = 'environment',
  EXTERNAL = 'external',
  // VIRTUAL_{facing} is for labeling video device for configuring extra stream
  // from corresponding {facing} video device.
  VIRTUAL_USER = 'virtual_user',
  VIRTUAL_ENV = 'virtual_environment',
  VIRTUAL_EXT = 'virtual_external',
  UNKNOWN = 'unknown',
}

export enum ViewName {
  CAMERA = 'view-camera',
  CROP_DOCUMENT = 'view-crop-document',
  DOCUMENT_MODE_DIALOG = 'view-document-mode-dialog',
  EXPERT_SETTINGS = 'view-expert-settings',
  FLASH = 'view-flash',
  GRID_SETTINGS = 'view-grid-settings',
  MESSAGE_DIALOG = 'view-message-dialog',
  PHOTO_RESOLUTION_SETTINGS = 'view-photo-resolution-settings',
  PTZ_PANEL = 'view-ptz-panel',
  RESOLUTION_SETTINGS = 'view-resolution-settings',
  REVIEW = 'view-review',
  SETTINGS = 'view-settings',
  SPLASH = 'view-splash',
  TIMER_SETTINGS = 'view-timer-settings',
  VIDEO_RESOLUTION_SETTINGS = 'view-video-resolution-settings',
  WARNING = 'view-warning',
}

export enum VideoType {
  MP4 = 'mp4',
  GIF = 'gif',
}

export enum Rotation {
  ANGLE_0 = 0,
  ANGLE_90 = 90,
  ANGLE_180 = 180,
  ANGLE_270 = 270,
}

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
  CAMERA_SWITCHING = 'camera-switching',
  GIF_CAPTURE_POST_PROCESSING = 'gif-capture-post-processing',
  LAUNCHING_FROM_LAUNCH_APP_COLD = 'launching-from-launch-app-cold',
  LAUNCHING_FROM_LAUNCH_APP_WARM = 'launching-from-launch-app-warm',
  LAUNCHING_FROM_WINDOW_CREATION = 'launching-from-window-creation',
  MODE_SWITCHING = 'mode-switching',
  PHOTO_CAPTURE_POST_PROCESSING = 'photo-capture-post-processing',
  PHOTO_CAPTURE_SHUTTER = 'photo-capture-shutter',
  PHOTO_TAKING = 'photo-taking',
  PORTRAIT_MODE_CAPTURE_POST_PROCESSING =
      'portrait-mode-capture-post-processing',
  VIDEO_CAPTURE_POST_PROCESSING = 'video-capture-post-processing',
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
}

export interface PerfEntry {
  event: PerfEvent;
  duration: number;
  perfInfo?: PerfInformation;
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
  private expired = false;

  constructor(
      readonly video: HTMLVideoElement, readonly onExpired: Promise<void>) {
    (async () => {
      await this.onExpired;
      this.expired = true;
    })();
  }

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
    return this.expired;
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
  BROKEN_THUMBNAIL = 'broken-thumbnail',
  DEVICE_INFO_UPDATE_FAILURE = 'device-info-update-failure',
  DEVICE_NOT_EXIST = 'device-not-exist',
  EMPTY_FILE = 'empty-file',
  FILE_SYSTEM_FAILURE = 'file-system-failure',
  FRAME_ROTATION_NOT_DISABLED = 'frame-rotation-not-disabled',
  HANDLE_CAMERA_RESULT_FAILURE = 'handle-camera-result-failure',
  IDLE_DETECTOR_FAILURE = 'idle-detector-failure',
  INVALID_REVIEW_UI_STATE = 'invalid-review-ui-state',
  METADATA_MAPPING_FAILURE = 'metadata-mapping-failure',
  MULTIPLE_STREAMS_FAILURE = 'multiple-streams-failure',
  NO_AVAILABLE_LEVEL = 'no-available-level',
  PERF_METRICS_FAILURE = 'perf-metrics-failure',
  PRELOAD_IMAGE_FAILURE = 'preload-image-failure',
  SET_FPS_RANGE_FAILURE = 'set-fps-range-failure',
  START_CAMERA_FAILURE = 'start-camera-failure',
  START_CAPTURE_FAILURE = 'start-capture-failure',
  STOP_CAPTURE_FAILURE = 'stop-capture-failure',
  UNCAUGHT_PROMISE = 'uncaught-promise',
  UNKNOWN_FACING = 'unknown-facing',
  UNSAFE_INTEGER = 'unsafe-integer',
  UNSUPPORTED_PROTOCOL = 'unsupported-protocol',
}

/**
 * Error level used in ERROR metrics.
 */
export enum ErrorLevel {
  WARNING = 'WARNING',
  ERROR = 'ERROR',
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
 * Throws when the portrait mode fails to detect a human face.
 */
export class PortraitModeProcessError extends Error {
  constructor(message = 'No human face detected in the scene') {
    super(message);
    this.name = this.constructor.name;
  }
}
