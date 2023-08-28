// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertEnumVariant,
  assertExists,
  assertInstanceof,
} from '../assert.js';
import * as dom from '../dom.js';
import {reportError} from '../error.js';
import * as expert from '../expert.js';
import {FaceOverlay} from '../face.js';
import {Point} from '../geometry.js';
import {DeviceOperator, parseMetadata} from '../mojo/device_operator.js';
import {
  AndroidControlAeAntibandingMode,
  AndroidControlAeMode,
  AndroidControlAeState,
  AndroidControlAfMode,
  AndroidControlAfState,
  AndroidControlAwbMode,
  AndroidControlAwbState,
  AndroidStatisticsFaceDetectMode,
  CameraMetadata,
  CameraMetadataEntry,
  CameraMetadataTag,
  StreamType,
} from '../mojo/type.js';
import {
  closeEndpoint,
  MojoEndpoint,
} from '../mojo/util.js';
import * as nav from '../nav.js';
import * as state from '../state.js';
import {
  ErrorLevel,
  ErrorType,
  Facing,
  getVideoTrackSettings,
  PreviewVideo,
  Resolution,
} from '../type.js';
import * as util from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {
  StreamConstraints,
  toMediaStreamConstraints,
} from './stream_constraints.js';

/**
 * Creates a controller for the video preview of Camera view.
 */
export class Preview {
  /**
   * Video element to capture the stream.
   */
  private video = dom.get('#preview-video', HTMLVideoElement);

  /**
   * The observer endpoint for preview metadata.
   */
  private metadataObserver: MojoEndpoint|null = null;

  /**
   * The face overlay for showing faces over preview.
   */
  private faceOverlay: FaceOverlay|null = null;

  /**
   * The observer to monitor average FPS of the preview stream.
   */
  private fpsObserver: util.FpsObserver|null = null;

  /**
   * Current active stream.
   */
  private streamInternal: MediaStream|null = null;

  /**
   * Watchdog for stream-end.
   */
  private watchdog: number|null = null;

  /**
   * Unique marker for the current applying focus.
   */
  private focusMarker: symbol|null = null;

  private facing: Facing|null = null;

  private deviceId: string|null = null;

  private vidPid: string|null = null;

  private isSupportPTZInternal = false;

  /**
   * Map from device id to constraints to reset default PTZ setting.
   */
  private readonly deviceDefaultPTZ =
      new Map<string, MediaTrackConstraintSet>();

  private constraints: StreamConstraints|null = null;

  private onPreviewExpired: WaitableEvent|null = null;

  private enableFaceOverlay = false;

  /**
   * @param onNewStreamNeeded Callback to request new stream.
   */
  constructor(private readonly onNewStreamNeeded: () => Promise<void>) {
    expert.addObserver(
        expert.ExpertOption.SHOW_METADATA, () => this.updateShowMetadata());
  }

  getVideo(): PreviewVideo {
    return new PreviewVideo(this.video, assertExists(this.onPreviewExpired));
  }

  /**
   * Current active stream.
   */
  get stream(): MediaStream {
    return assertInstanceof(this.streamInternal, MediaStream);
  }

  getVideoElement(): HTMLVideoElement {
    return this.video;
  }

  private getVideoTrack(): MediaStreamTrack {
    return this.stream.getVideoTracks()[0];
  }

  getFacing(): Facing {
    return assertEnumVariant(Facing, this.facing);
  }

  getDeviceId(): string|null {
    return this.deviceId;
  }

  /**
   * USB camera vid:pid identifier of the opened stream.
   *
   * @return Identifier formatted as "vid:pid" or null for non-USB camera.
   */
  getVidPid(): string|null {
    return this.vidPid;
  }

  getConstraints(): StreamConstraints {
    assert(this.constraints !== null);
    return this.constraints;
  }

  private updateFacing() {
    const {facingMode} = this.getVideoTrack().getSettings();
    switch (facingMode) {
      case 'user':
        this.facing = Facing.USER;
        return;
      case 'environment':
        this.facing = Facing.ENVIRONMENT;
        return;
      default:
        this.facing = Facing.EXTERNAL;
        return;
    }
  }

  private async updatePTZ() {
    const deviceOperator = DeviceOperator.getInstance();
    const {pan, tilt, zoom} = this.getVideoTrack().getCapabilities();

    this.isSupportPTZInternal = (() => {
      if (pan === undefined && tilt === undefined && zoom === undefined) {
        return false;
      }
      if (deviceOperator === null) {
        // Enable PTZ on fake camera for testing.
        return true;
      }
      if (this.facing === Facing.EXTERNAL) {
        return true;
      } else if (expert.isEnabled(expert.ExpertOption.ENABLE_PTZ_FOR_BUILTIN)) {
        return true;
      }

      return false;
    })();

    if (!this.isSupportPTZInternal) {
      return;
    }

    const {deviceId} = getVideoTrackSettings(this.getVideoTrack());
    if (this.deviceDefaultPTZ.has(deviceId)) {
      return;
    }

    const defaultConstraints: MediaTrackConstraintSet = {};
    if (deviceOperator === null) {
      // VCD of fake camera will always reset to default when first opened. Use
      // current value at first open as default.
      if (pan !== undefined) {
        defaultConstraints.pan = pan;
      }
      if (tilt !== undefined) {
        defaultConstraints.tilt = tilt;
      }
      if (zoom !== undefined) {
        defaultConstraints.zoom = zoom;
      }
    } else {
      if (pan !== undefined) {
        defaultConstraints.pan = await deviceOperator.getPanDefault(deviceId);
      }
      if (tilt !== undefined) {
        defaultConstraints.tilt = await deviceOperator.getTiltDefault(deviceId);
      }
      if (zoom !== undefined) {
        defaultConstraints.zoom = await deviceOperator.getZoomDefault(deviceId);
      }
    }
    this.deviceDefaultPTZ.set(deviceId, defaultConstraints);
  }

  /**
   * If the preview camera support PTZ controls.
   */
  isSupportPTZ(): boolean {
    return this.isSupportPTZInternal;
  }

  async resetPTZ(): Promise<void> {
    if (this.streamInternal === null || !this.isSupportPTZInternal) {
      return;
    }
    const {deviceId} = getVideoTrackSettings(this.getVideoTrack());
    const defaultPTZ = this.deviceDefaultPTZ.get(deviceId);
    assert(defaultPTZ !== undefined);
    await this.getVideoTrack().applyConstraints({advanced: [defaultPTZ]});
  }

  /**
   * Preview resolution.
   */
  getResolution(): Resolution {
    const {videoWidth, videoHeight} = this.video;
    return new Resolution(videoWidth, videoHeight);
  }

  toString(): string {
    const {videoWidth, videoHeight} = this.video;
    return videoHeight > 0 ? `${videoWidth} x ${videoHeight}` : '';
  }

  /**
   * Sets video element's source.
   *
   * @param stream Stream to be the source.
   */
  private async setSource(stream: MediaStream): Promise<void> {
    const tpl = util.instantiateTemplate('#preview-video-template');
    const video = dom.getFrom(tpl, 'video', HTMLVideoElement);
    await new Promise<void>((resolve) => {
      function handler() {
        video.removeEventListener('canplay', handler);
        resolve();
      }
      video.addEventListener('canplay', handler);
      video.srcObject = stream;
    });
    await video.play();
    assert(this.video.parentElement !== null);
    this.video.parentElement.replaceChild(tpl, this.video);
    this.video.srcObject = null;
    this.video = video;
    video.addEventListener('resize', () => this.onIntrinsicSizeChanged());
    video.addEventListener(
        'click',
        (event) => this.onFocusClicked(assertInstanceof(event, MouseEvent)));
    // Disable right click on video which let user show video control.
    video.addEventListener('contextmenu', (event) => event.preventDefault());
    return this.onIntrinsicSizeChanged();
  }

  private isStreamAlive(): boolean {
    assert(this.streamInternal !== null);
    return this.streamInternal.getVideoTracks().length !== 0 &&
        this.streamInternal.getVideoTracks()[0].readyState !== 'ended';
  }

  private clearWatchdog() {
    if (this.watchdog !== null) {
      clearInterval(this.watchdog);
      this.watchdog = null;
    }
  }

  /**
   * Opens preview stream.
   *
   * @param constraints Constraints of preview stream.
   * @return Promise resolved to opened preview stream.
   */
  async open(constraints: StreamConstraints): Promise<MediaStream> {
    this.constraints = constraints;
    this.streamInternal = await navigator.mediaDevices.getUserMedia(
        toMediaStreamConstraints(constraints));
    try {
      await this.setSource(this.streamInternal);
      // Use a watchdog since the stream.onended event is unreliable in the
      // recent version of Chrome. As of 55, the event is still broken.
      this.watchdog = setInterval(() => {
        if (!this.isStreamAlive()) {
          this.clearWatchdog();
          const deviceOperator = DeviceOperator.getInstance();
          if (deviceOperator !== null && this.deviceId !== null) {
            deviceOperator.dropConnection(this.deviceId);
          }
          this.onNewStreamNeeded();
        }
      }, 100);
      this.updateFacing();
      this.deviceId = getVideoTrackSettings(this.getVideoTrack()).deviceId;
      await this.updatePTZ();

      this.enableFaceOverlay = false;
      const deviceOperator = DeviceOperator.getInstance();
      if (deviceOperator !== null) {
        const {deviceId} = getVideoTrackSettings(this.getVideoTrack());
        const isSuccess =
            await deviceOperator.setCameraFrameRotationEnabledAtSource(
                deviceId, false);
        if (!isSuccess) {
          reportError(
              ErrorType.FRAME_ROTATION_NOT_DISABLED, ErrorLevel.WARNING,
              new Error(
                  'Cannot disable camera frame rotation. ' +
                  'The camera is probably being used by another app.'));
        } else {
          this.enableFaceOverlay = true;
        }
        this.vidPid = await deviceOperator.getVidPid(deviceId);
      }
      this.updateShowMetadata();

      assert(
          this.onPreviewExpired === null || this.onPreviewExpired.isSignaled());
      this.onPreviewExpired = new WaitableEvent();
      state.set(state.State.STREAMING, true);
    } catch (e) {
      this.close();
      throw e;
    }
    return this.streamInternal;
  }

  /**
   * Closes the preview.
   */
  close(): void {
    this.clearWatchdog();
    // Pause video element to avoid black frames during transition.
    this.video.pause();
    this.disableShowMetadata();
    this.enableFaceOverlay = false;
    if (this.streamInternal !== null && this.isStreamAlive()) {
      const track = this.getVideoTrack();
      const {deviceId} = getVideoTrackSettings(track);
      track.stop();
      const deviceOperator = DeviceOperator.getInstance();
      if (deviceOperator !== null) {
        deviceOperator.dropConnection(deviceId);
      }
      assert(this.onPreviewExpired !== null);
    }
    this.streamInternal = null;

    if (this.onPreviewExpired !== null) {
      this.onPreviewExpired.signal();
    }
    state.set(state.State.STREAMING, false);
  }

  /**
   * Updates preview whether to show preview metadata or not.
   */
  private updateShowMetadata() {
    if (expert.isEnabled(expert.ExpertOption.SHOW_METADATA)) {
      this.enableShowMetadata();
    } else {
      this.disableShowMetadata();
    }
  }

  /**
   * Creates an image blob of the current frame.
   */
  toImage(): Promise<Blob> {
    const {canvas, ctx} = util.newDrawingCanvas(
        {width: this.video.videoWidth, height: this.video.videoHeight});
    ctx.drawImage(this.video, 0, 0);
    return util.canvasToJpegBlob(canvas);
  }

  /**
   * Displays preview metadata on preview screen.
   */
  private async enableShowMetadata(): Promise<void> {
    if (this.streamInternal === null) {
      return;
    }

    for (const element of dom.getAll('.metadata.value', HTMLElement)) {
      element.style.display = 'none';
    }

    function displayCategory(selector: string, enabled: boolean) {
      dom.get(selector, HTMLElement).classList.toggle('mode-on', enabled);
    }

    function showValue(selector: string, val: string) {
      const element = dom.get(selector, HTMLElement);
      element.style.display = '';
      element.textContent = val;
    }

    function buildInverseLookupFunction<T extends number>(
        enumType: Record<string, T|string>, prefix: string): (key: number) =>
        string {
      const map = new Map<number, string>();
      const obj = util.getNumberEnumMapping(enumType);
      for (const [key, val] of Object.entries(obj)) {
        if (!key.startsWith(prefix)) {
          continue;
        }
        if (map.has(val)) {
          reportError(
              ErrorType.METADATA_MAPPING_FAILURE, ErrorLevel.ERROR,
              new Error(`Duplicated value: ${val}`));
          continue;
        }
        map.set(val, key.slice(prefix.length));
      }
      return (key: number) => {
        const val = map.get(key);
        assert(val !== undefined);
        return val;
      };
    }

    const afStateNameLookup = buildInverseLookupFunction(
        AndroidControlAfState, 'ANDROID_CONTROL_AF_STATE_');
    const aeStateNameLookup = buildInverseLookupFunction(
        AndroidControlAeState, 'ANDROID_CONTROL_AE_STATE_');
    const awbStateNameLookup = buildInverseLookupFunction(
        AndroidControlAwbState, 'ANDROID_CONTROL_AWB_STATE_');
    const aeAntibandingModeNameLookup = buildInverseLookupFunction(
        AndroidControlAeAntibandingMode,
        'ANDROID_CONTROL_AE_ANTIBANDING_MODE_');

    let sensorSensitivity: number|null = null;
    let sensorSensitivityBoost = 100;
    function getSensitivity() {
      if (sensorSensitivity === null) {
        return 'N/A';
      }
      return sensorSensitivity * sensorSensitivityBoost / 100;
    }

    const tag = CameraMetadataTag;
    const metadataEntryHandlers: Record<string, (values: number[]) => void> = {
      [tag.ANDROID_LENS_FOCUS_DISTANCE]: ([value]) => {
        if (value === 0) {
          // Fixed-focus camera
          return;
        }
        const focusDistance = (100 / value).toFixed(1);
        showValue('#preview-focus-distance', `${focusDistance} cm`);
      },
      [tag.ANDROID_CONTROL_AF_STATE]: ([value]) => {
        showValue('#preview-af-state', afStateNameLookup(value));
      },
      [tag.ANDROID_SENSOR_SENSITIVITY]: ([value]) => {
        sensorSensitivity = value;
        const sensitivity = getSensitivity();
        showValue('#preview-sensitivity', `ISO ${sensitivity}`);
      },
      [tag.ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST]: ([value]) => {
        sensorSensitivityBoost = value;
        const sensitivity = getSensitivity();
        showValue('#preview-sensitivity', `ISO ${sensitivity}`);
      },
      [tag.ANDROID_SENSOR_EXPOSURE_TIME]: ([value]) => {
        const shutterSpeed = Math.round(1e9 / value);
        showValue('#preview-exposure-time', `1/${shutterSpeed}`);
      },
      [tag.ANDROID_SENSOR_FRAME_DURATION]: ([value]) => {
        const frameFrequency = Math.round(1e9 / value);
        showValue('#preview-frame-duration', `${frameFrequency} Hz`);
      },
      [tag.ANDROID_CONTROL_AE_ANTIBANDING_MODE]: ([value]) => {
        showValue(
            '#preview-ae-antibanding-mode', aeAntibandingModeNameLookup(value));
      },
      [tag.ANDROID_CONTROL_AE_STATE]: ([value]) => {
        showValue('#preview-ae-state', aeStateNameLookup(value));
      },
      [tag.ANDROID_COLOR_CORRECTION_GAINS]: ([valueRed, , , valueBlue]) => {
        const wbGainRed = valueRed.toFixed(2);
        showValue('#preview-wb-gain-red', `${wbGainRed}x`);
        const wbGainBlue = valueBlue.toFixed(2);
        showValue('#preview-wb-gain-blue', `${wbGainBlue}x`);
      },
      [tag.ANDROID_CONTROL_AWB_STATE]: ([value]) => {
        showValue('#preview-awb-state', awbStateNameLookup(value));
      },
      [tag.ANDROID_CONTROL_AF_MODE]: ([value]) => {
        displayCategory(
            '#preview-af',
            value !== AndroidControlAfMode.ANDROID_CONTROL_AF_MODE_OFF);
      },
      [tag.ANDROID_CONTROL_AE_MODE]: ([value]) => {
        displayCategory(
            '#preview-ae',
            value !== AndroidControlAeMode.ANDROID_CONTROL_AE_MODE_OFF);
      },
      [tag.ANDROID_CONTROL_AWB_MODE]: ([value]) => {
        displayCategory(
            '#preview-awb',
            value !== AndroidControlAwbMode.ANDROID_CONTROL_AWB_MODE_OFF);
      },
    };

    // These should be per session static information and we don't need to
    // recalculate them in every callback.
    const {videoWidth, videoHeight} = this.video;
    const resolution = `${videoWidth}x${videoHeight}`;
    const videoTrack = this.getVideoTrack();
    const deviceName = videoTrack.label;
    const deviceOperator = DeviceOperator.getInstance();
    if (deviceOperator === null) {
      return;
    }

    this.fpsObserver = new util.FpsObserver(this.video);

    const {deviceId} = getVideoTrackSettings(videoTrack);
    const activeArraySize = await deviceOperator.getActiveArraySize(deviceId);
    const cameraFrameRotation =
        await deviceOperator.getCameraFrameRotation(deviceId);
    if (this.enableFaceOverlay) {
      this.faceOverlay =
          new FaceOverlay(activeArraySize, cameraFrameRotation, deviceId);
    }
    const updateFace =
        (mode: AndroidStatisticsFaceDetectMode, rects: number[]) => {
          if (mode ===
              AndroidStatisticsFaceDetectMode
                  .ANDROID_STATISTICS_FACE_DETECT_MODE_OFF) {
            dom.get('#preview-num-faces', HTMLDivElement).style.display =
                'none';
            this.faceOverlay?.clearRects();
            return;
          }
          assert(rects.length % 4 === 0);
          const numFaces = rects.length / 4;
          const label = numFaces >= 2 ? 'Faces' : 'Face';
          showValue('#preview-num-faces', `${numFaces} ${label}`);
          this.faceOverlay?.show(rects);
        };

    const callback = (metadata: CameraMetadata) => {
      showValue('#preview-resolution', resolution);
      showValue('#preview-device-name', deviceName);
      if (this.fpsObserver !== null) {
        const fps = this.fpsObserver.getAverageFps();
        if (fps !== null) {
          showValue('#preview-fps', `${fps.toFixed(0)} FPS`);
        }
      }

      let faceMode = AndroidStatisticsFaceDetectMode
                         .ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
      let faceRects: number[] = [];

      function tryParseFaceEntry(entry: CameraMetadataEntry) {
        switch (entry.tag) {
          case tag.ANDROID_STATISTICS_FACE_DETECT_MODE: {
            const data = parseMetadata(entry);
            assert(data.length === 1);
            faceMode = data[0];
            return true;
          }
          case tag.ANDROID_STATISTICS_FACE_RECTANGLES: {
            faceRects = parseMetadata(entry);
            return true;
          }
          default:
            return false;
        }
      }

      assert(metadata.entries !== undefined);
      for (const entry of metadata.entries) {
        if (entry.count === 0) {
          continue;
        }
        if (tryParseFaceEntry(entry)) {
          continue;
        }
        const handler = metadataEntryHandlers[entry.tag];
        if (handler === undefined) {
          continue;
        }
        handler(parseMetadata(entry));
      }

      // We always need to run updateFace() even if face rectangles are obsent
      // in the metadata, which may happen if there is no face detected.
      updateFace(faceMode, faceRects);
    };

    this.metadataObserver = await deviceOperator.addMetadataObserver(
        deviceId, callback, StreamType.PREVIEW_OUTPUT);
  }

  /**
   * Hides display preview metadata on preview screen.
   */
  private disableShowMetadata(): void {
    if (this.streamInternal === null || this.metadataObserver === null) {
      return;
    }

    closeEndpoint(this.metadataObserver);
    this.metadataObserver = null;

    if (this.faceOverlay !== null) {
      this.faceOverlay.clear();
      this.faceOverlay = null;
    }

    if (this.fpsObserver !== null) {
      this.fpsObserver.stop();
      this.fpsObserver = null;
    }
  }

  /**
   * Handles changed intrinsic size (first loaded or orientation changes).
   */
  private onIntrinsicSizeChanged(): void {
    if (this.video.videoWidth !== 0 && this.video.videoHeight !== 0) {
      nav.layoutShownViews();
    }
    this.cancelFocus();
  }

  /**
   * Applies point of interest to the stream.
   *
   * @param point The point in normalize coordidate system, which means both
   *     |x| and |y| are in range [0, 1).
   */
  setPointOfInterest(point: Point): Promise<void> {
    const constraints = {
      advanced: [{pointsOfInterest: [{x: point.x, y: point.y}]}],
    };
    const track = this.getVideoTrack();
    return track.applyConstraints(constraints);
  }

  /**
   * Handles clicking for focus.
   *
   * @param event Click event.
   */
  private onFocusClicked(event: MouseEvent) {
    this.cancelFocus();
    const marker = Symbol();
    this.focusMarker = marker;
    (async () => {
      try {
        // Normalize to square space coordinates by W3C spec.
        const x = event.offsetX / this.video.offsetWidth;
        const y = event.offsetY / this.video.offsetHeight;
        await this.setPointOfInterest(new Point(x, y));
      } catch {
        // The device might not support setting pointsOfInterest. Ignore the
        // error and return.
        return;
      }
      if (marker !== this.focusMarker) {
        return;  // Focus was cancelled.
      }
      const aim = dom.get('#preview-focus-aim', HTMLElement);
      const clone = assertInstanceof(aim.cloneNode(true), HTMLElement);
      clone.style.left = `${event.offsetX + this.video.offsetLeft}px`;
      clone.style.top = `${event.offsetY + this.video.offsetTop}px`;
      clone.hidden = false;
      assert(aim.parentElement !== null);
      aim.parentElement.replaceChild(clone, aim);
    })();
  }

  /**
   * Cancels the currently applied focus.
   */
  private cancelFocus() {
    this.focusMarker = null;
    const aim = dom.get('#preview-focus-aim', HTMLElement);
    aim.hidden = true;
  }
}
