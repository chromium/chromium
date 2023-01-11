// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists, assertInstanceof} from '../assert.js';
import * as dom from '../dom.js';
import * as localStorage from '../models/local_storage.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import {Facing, Resolution} from '../type.js';
import {windowController} from '../window_controller.js';

import {getUIComponent, UIComponent} from './cca_type.js';

/**
 * Returns HTMLVideoElement for the preview video.
 */
function getPreviewVideo(): HTMLVideoElement {
  const previewVideoInfo = getUIComponent(UIComponent.PREVIEW_VIDEO);
  return dom.get(previewVideoInfo.selector, HTMLVideoElement);
}

function getPreviewVideoStream(): MediaStream {
  return assertInstanceof(getPreviewVideo().srcObject, MediaStream);
}

function getPreviewVideoTrack(): MediaStreamTrack {
  const track = getPreviewVideoStream().getVideoTracks()[0];
  return assertInstanceof(track, MediaStreamTrack);
}

/**
 * Return test functionalities to be used in Tast automation test.
 */
export class CCATest {
  /**
   * Checks if mojo connection could be constructed without error. In this check
   * we only check if the path works and does not check for the correctness of
   * each mojo calls.
   *
   * @param shouldSupportDeviceOperator True if the device should support
   * DeviceOperator.
   * @return The promise resolves successfully if the check passes.
   */
  static async checkMojoConnection(shouldSupportDeviceOperator: boolean):
      Promise<void> {
    // Checks if ChromeHelper works. It should work on all devices.
    const chromeHelper = ChromeHelper.getInstance();
    await chromeHelper.isTabletMode();

    const isDeviceOperatorSupported = DeviceOperator.isSupported();
    if (shouldSupportDeviceOperator !== isDeviceOperatorSupported) {
      throw new Error(`DeviceOperator support mismatch. Expected: ${
          shouldSupportDeviceOperator} Actual: ${isDeviceOperatorSupported}`);
    }

    // Checks if DeviceOperator works on v3 devices.
    if (isDeviceOperatorSupported) {
      const deviceOperator = DeviceOperator.getInstance();
      assert(deviceOperator !== null, 'Failed to get deviceOperator instance.');
      const devices = (await navigator.mediaDevices.enumerateDevices())
                          .filter(({kind}) => kind === 'videoinput');
      await deviceOperator.getCameraFacing(devices[0].deviceId);
    }
  }

  /**
   * Focuses the window.
   */
  static focusWindow(): Promise<void> {
    return windowController.focus();
  }

  /**
   * Makes the window fullscreen.
   */
  static fullscreenWindow(): Promise<void> {
    return windowController.fullscreen();
  }

  /**
   * Gets device id of current active camera device.
   */
  static getDeviceId(): string {
    const deviceId = getPreviewVideoTrack().getSettings().deviceId;
    return assertExists(deviceId, 'Invalid deviceId');
  }

  /**
   * Gets facing of current active camera device.
   *
   * @return The facing string 'user', 'environment', 'external'. Returns
   *     'unknown' if current device does not support device operator.
   */
  static async getFacing(): Promise<string> {
    const track = getPreviewVideoTrack();
    const deviceOperator = DeviceOperator.getInstance();
    if (!deviceOperator) {
      const facing = track.getSettings().facingMode;
      return facing ?? 'unknown';
    }

    const deviceId = CCATest.getDeviceId();
    const facing = await deviceOperator.getCameraFacing(deviceId);
    switch (facing) {
      case Facing.USER:
      case Facing.ENVIRONMENT:
      case Facing.EXTERNAL:
        return facing;
      default:
        throw new Error(`Unexpected CameraFacing value: ${facing}`);
    }
  }

  /**
   * Gets number of camera devices.
   */
  static async getNumOfCameras(): Promise<number> {
    const devices = await navigator.mediaDevices.enumerateDevices();
    return devices
        .filter(
            (d) => d.kind === 'videoinput' &&
                !d.label.startsWith('Virtual Camera'))
        .length;
  }

  /**
   * Creates a canvas which renders a frame from the preview video.
   *
   * @return Context from the created canvas.
   */
  static getPreviewFrame(): OffscreenCanvasRenderingContext2D {
    const video = getPreviewVideo();
    const canvas = new OffscreenCanvas(video.videoWidth, video.videoHeight);
    const ctx = canvas.getContext('2d');
    assert(ctx !== null, 'Failed to get canvas context.');
    ctx.drawImage(video, 0, 0);
    return ctx;
  }

  /**
   * Gets resolution of the preview video.
   */
  static getPreviewResolution(): Resolution {
    const video = getPreviewVideo();
    return new Resolution(video.videoWidth, video.videoHeight);
  }

  /**
   * Returns the screen orientation.
   */
  static getScreenOrientation(): OrientationType {
    return window.screen.orientation.type;
  }

  /**
   * Checks whether the preview video stream has been set and the stream status
   * is active.
   *
   * @return Whether the preview video is active.
   */
  static isVideoActive(): boolean {
    const video = getPreviewVideo();
    return video.srcObject instanceof MediaStream && video.srcObject.active;
  }

  /**
   * Maximizes the window.
   */
  static maximizeWindow(): Promise<void> {
    return windowController.maximize();
  }

  /**
   * Minimizes the window.
   */
  static minimizeWindow(): Promise<void> {
    return windowController.minimize();
  }

  /**
   * Removes all the cached data in chrome.storage.local.
   */
  static removeCacheData(): void {
    return localStorage.clear();
  }

  /**
   * Restores the window and leaves maximized/minimized/fullscreen state.
   */
  static restoreWindow(): Promise<void> {
    return windowController.restore();
  }
}
