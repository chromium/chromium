// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '../assert.js';
import * as dom from '../dom.js';
import * as localStorage from '../models/local_storage.js';
import * as state from '../state.js';
import {
  Facing,
  Mode,
  Resolution,
  ResolutionList,  // eslint-disable-line no-unused-vars
} from '../type.js';

// eslint-disable-next-line no-unused-vars
import {Camera3DeviceInfo} from './camera3_device_info.js';
// eslint-disable-next-line no-unused-vars
import {StreamConstraints} from './stream_constraints.js';

/**
 * Candidate of capturing with specified photo or video resolution and
 * constraints-candidates it corresponding preview.
 * Video/photo capture resolution and the constraints-candidates of its
 * corresponding preview stream.
 * @typedef {{
 *   resolution: !Resolution,
 *   previewCandidates: !Array<!StreamConstraints>
 * }}
 */
export let CaptureCandidate;

/**
 * Controller for managing preference of capture settings and generating a list
 * of stream constraints-candidates sorted by user preference.
 * @abstract
 */
export class ConstraintsPreferrer {
  /**
   * @param {function(): !Promise} doReconfigureStream Trigger stream
   *     reconfiguration to reflect changes in user preferred settings.
   * @protected
   */
  constructor(doReconfigureStream) {
    /**
     * @type {function(): !Promise}
     * @protected
     */
    this.doReconfigureStream_ = doReconfigureStream;

    /**
     * Map saving resolution preference that each of its key as device id and
     * value to be preferred width, height of resolution of that video device.
     * @type {!Map<string, !Resolution>}
     * @protected
     */
    this.prefResolution_ = new Map();

    /**
     * Device id of currently working video device.
     * @type {?string}
     * @protected
     */
    this.deviceId_ = null;

    /**
     * Maps video device id to all of available capture resolutions supported by
     * that video device.
     * @type {!Map<string, !ResolutionList>}
     * @protected
     */
    this.supportedResolutions_ = new Map();

    /**
     * Listener for changes of preferred resolution used on particular video
     * device.
     * @type {function(string, !Resolution): void}
     * @protected
     */
    this.preferredResolutionChangeListener_ = () => {};
  }

  /**
   * Restores saved preferred capture resolution per video device.
   * @param {string} key Key of local storage saving preferences.
   * @protected
   */
  restoreResolutionPreference_(key) {
    const preference = localStorage.getObject(key);
    this.prefResolution_ = new Map();
    for (const [deviceId, {width, height}] of Object.entries(preference)) {
      this.prefResolution_.set(deviceId, new Resolution(width, height));
    }
  }

  /**
   * Saves preferred capture resolution per video device.
   * @param {string} key Key of local storage saving preferences.
   * @protected
   */
  saveResolutionPreference_(key) {
    localStorage.set(key, Object.fromEntries(this.prefResolution_));
  }

  /**
   * Gets user preferred capture resolution for a specific device.
   * @param {string} deviceId Device id of the device.
   * @return {?Resolution} Returns preferred resolution or null if no preferred
   *     resolution found in user preference.
   */
  getPrefResolution(deviceId) {
    return this.prefResolution_.get(deviceId) || null;
  }

  /**
   * Updates with new video device information.
   * @param {!Array<!Camera3DeviceInfo>} devices
   * @abstract
   */
  updateDevicesInfo(devices) {
    assertNotReached();
  }

  /**
   * Updates values according to currently working video device and capture
   * settings.
   * @param {string} deviceId Device id of video device to be updated.
   * @param {!MediaStream} stream Currently active preview stream.
   * @param {!Facing} facing Camera facing of video device to be updated.
   * @param {!Resolution} resolution Resolution to be updated to.
   * @abstract
   */
  updateValues(deviceId, stream, facing, resolution) {
    assertNotReached();
  }

  /**
   * Gets all available candidates for capturing under this controller and its
   * corresponding preview constraints for the specified video device. Returned
   * resolutions and constraints candidates are both sorted in desired trying
   * order.
   * @abstract
   * @param {string} deviceId Device id of video device.
   * @return {!Array<!CaptureCandidate>} Capture resolution and its preview
   *     constraints-candidates.
   */
  getSortedCandidates(deviceId) {
    assertNotReached();
  }

  /**
   * Gets capture resolution supported by video device with given device id.
   * @param {string} deviceId
   * @return {!ResolutionList}
   */
  getSupportedResolutions(deviceId) {
    // Guarding from fake camera code path calling this function.
    assert(this.supportedResolutions_.get(deviceId) !== undefined);
    return this.supportedResolutions_.get(deviceId);
  }

  /**
   * Changes user preferred capture resolution.
   * @abstract
   * @param {string} deviceId Device id of the video device to be changed.
   * @param {!Resolution} resolution Preferred capture resolution.
   */
  changePreferredResolution(deviceId, resolution) {
    assertNotReached();
  }

  /**
   * Sets listener for changes of preferred resolution used in taking photo on
   * particular video device.
   * @param {function(string, !Resolution): void} listener
   */
  setPreferredResolutionChangeListener(listener) {
    this.preferredResolutionChangeListener_ = listener;
  }

  /**
   * Sorts the preview resolution (Rp) according to the capture resolution
   * (Rc) and the screen size (Rs) with the following orders:
   * If |Rc| <= |Rs|:
   *   1. All |Rp| <= |Rc|, and the larger, the better.
   *   2. All |Rp| > |Rc|, and the smaller, the better.
   *
   * If |Rc| > |Rs|:
   *   1. All |Rp| where |Rs| <= |Rp| <= |Rc|, and the smaller, the
   *   better.
   *   2. All |Rp| < |Rs|, and the larger, the better.
   *   3. All |Rp| > |Rc|, and the smaller, the better.
   *
   * Note that generally we compare resolutions by their width. But since the
   * aspect ratio of |Rs| might be different from the |Rc| and |Rp|, we also
   * consider |screenHeight * captureAspectRatio| as a possible |Rs| and prefer
   * using the smaller one.
   *
   * @param {!ResolutionList} previewResolutions
   * @param {!Resolution} captureResolution
   * @return {!ResolutionList}
   * @protected
   */
  sortPreview_(previewResolutions, captureResolution) {
    if (previewResolutions.length === 0) {
      return [];
    }

    const screenWidth =
        Math.floor(window.screen.width * window.devicePixelRatio);
    const screenHeight =
        Math.floor(window.screen.height * window.devicePixelRatio);
    const aspectRatio = captureResolution.width / captureResolution.height;
    const Rs = Math.min(screenWidth, Math.floor(screenHeight * aspectRatio));
    const Rc = captureResolution.width;
    const cmpDescending = (r1, r2) => r2.width - r1.width;
    const cmpAscending = (r1, r2) => r1.width - r2.width;

    if (Rc <= Rs) {
      const notLargerThanR =
          previewResolutions.filter((r) => r.width <= Rc).sort(cmpDescending);
      const largerThanR =
          previewResolutions.filter((r) => r.width > Rc).sort(cmpAscending);
      return notLargerThanR.concat(largerThanR);
    } else {
      const betweenRsR =
          previewResolutions.filter((r) => Rs <= r.width && r.width <= Rc)
              .sort(cmpAscending);
      const smallerThanRs =
          previewResolutions.filter((r) => r.width < Rs).sort(cmpDescending);
      const largerThanR =
          previewResolutions.filter((r) => r.width > Rc).sort(cmpAscending);
      return betweenRsR.concat(smallerThanRs).concat(largerThanR);
    }
  }

  /**
   * Sorts prefer resolutions.
   * @param {!Resolution} prefR Preferred resolution.
   * @return {function(!CaptureCandidate, !CaptureCandidate): number} Return
   *     compare function for comparing based on preferred resolution.
   * @protected
   */
  getPreferResolutionSort_(prefR) {
    /*
     * @param {!CaptureCandidate} candidate
     * @param {!CaptureCandidate} candidate2
     * @return {number}
     */
    return ({resolution: r1}, {resolution: r2}) => {
      if (r1.equals(r2)) {
        return 0;
      }
      // Exactly the preferred resolution.
      if (r1.equals(prefR)) {
        return -1;
      }
      if (r2.equals(prefR)) {
        return 1;
      }

      // Aspect ratio same as preferred resolution.
      if (!r1.aspectRatioEquals(r2)) {
        if (r1.aspectRatioEquals(prefR)) {
          return -1;
        }
        if (r2.aspectRatioEquals(prefR)) {
          return 1;
        }
      }
      return r2.area - r1.area;
    };
  }

  /**
   * Groups resolutions with same ratio into same list.
   * @param {!ResolutionList} rs
   * @return {!Map<number, !ResolutionList>} Ratio as key, all resolutions
   *     with that ratio as value.
   */
  groupResolutionRatio_(rs) {
    /**
     * @param {!Resolution} r
     * @return {number}
     */
    const toSupportedPreviewRatio = (r) => {
      // Special aspect ratio mapping rule, see http://b/147986763.
      if (r.width === 848 && r.height === 480) {
        return (new Resolution(16, 9)).aspectRatio;
      }
      return r.aspectRatio;
    };

    const result = new Map();
    for (const r of rs) {
      const ratio = toSupportedPreviewRatio(r);
      result.set(ratio, result.get(ratio) || []);
      result.get(ratio).push(r);
    }
    return result;
  }
}

/**
 * All supported constant fps options of video recording.
 * @type {!Array<number>}
 */
const SUPPORTED_CONSTANT_FPS = [30, 60];

/**
 * Controller for handling video resolution preference.
 */
export class VideoConstraintsPreferrer extends ConstraintsPreferrer {
  /**
   * @param {function(): !Promise} doReconfigureStream
   * @public
   */
  constructor(doReconfigureStream) {
    super(doReconfigureStream);

    /**
     * Object saving information of device supported constant fps. Each of its
     * key as device id and value as an object mapping from resolution to all
     * constant fps options supported by that resolution.
     * @type {!Object<string, !Object<string, !Array<number>>>}
     * @private
     */
    this.constFpsInfo_ = {};

    /**
     * Object saving fps preference that each of its key as device id and value
     * as an object mapping from resolution to preferred constant fps for that
     * resolution.
     * @type {!Object<string, !Object<string, number>>}
     * @private
     */
    this.prefFpses_ = {};

    /**
     * @type {!HTMLInputElement}
     * @const
     * @private
     */
    this.toggleFps_ = dom.get('#toggle-fps', HTMLInputElement);

    /**
     * Currently in used recording resolution.
     * @type {!Resolution}
     * @protected
     */
    this.resolution_ = new Resolution(0, -1);
    this.restoreResolutionPreference_('deviceVideoResolution');
    this.restoreFpsPreference_();

    /**
     * Maps from device id as key to video and preview resolutions of
     * same aspect ratio supported by that video device as value.
     * @type {!Map<string, !Array<{videoRs: !ResolutionList, previewRs:
     *     !ResolutionList}>>}
     * @private
     */
    this.deviceVideoPreviewResolutionMap_ = new Map();

    this.toggleFps_.addEventListener('click', (event) => {
      if (!state.get(state.State.STREAMING) || state.get(state.State.TAKING)) {
        event.preventDefault();
      }
    });
    this.toggleFps_.addEventListener('change', (event) => {
      this.setPreferredConstFps_(
          /** @type {string} */ (this.deviceId_), this.resolution_,
          this.toggleFps_.checked ? 60 : 30);
      state.set(state.State.MODE_SWITCHING, true);
      let hasError = false;
      this.doReconfigureStream_()
          .catch((error) => {
            hasError = true;
            throw error;
          })
          .finally(
              () => state.set(state.State.MODE_SWITCHING, false, {hasError}));
    });
  }

  /**
   * Restores saved preferred fps per video resolution per video device.
   * @private
   */
  restoreFpsPreference_() {
    this.prefFpses_ = localStorage.getObject('deviceVideoFps');
  }

  /**
   * Saves preferred fps per video resolution per video device.
   * @private
   */
  saveFpsPreference_() {
    localStorage.set('deviceVideoFps', this.prefFpses_);
  }

  /**
   * @override
   */
  changePreferredResolution(deviceId, resolution) {
    this.prefResolution_.set(deviceId, resolution);
    this.saveResolutionPreference_('deviceVideoResolution');
    if (state.get(Mode.VIDEO) && deviceId === this.deviceId_) {
      this.doReconfigureStream_();
    } else {
      this.preferredResolutionChangeListener_(deviceId, resolution);
    }
  }

  /**
   * Sets the preferred fps used in video recording for particular video device
   * with particular resolution.
   * @param {string} deviceId Device id of video device to be set with.
   * @param {!Resolution} resolution Resolution to be set with.
   * @param {number} prefFps Preferred fps to be set with.
   * @private
   */
  setPreferredConstFps_(deviceId, resolution, prefFps) {
    if (!SUPPORTED_CONSTANT_FPS.includes(prefFps)) {
      return;
    }
    this.toggleFps_.checked = prefFps === 60;
    SUPPORTED_CONSTANT_FPS.forEach(
        (fps) => state.set(state.assertState(`fps-${fps}`), fps === prefFps));
    this.prefFpses_[deviceId] = this.prefFpses_[deviceId] || {};
    this.prefFpses_[deviceId][resolution.toString()] = prefFps;
    this.saveFpsPreference_();
  }

  /**
   * @override
   */
  updateDevicesInfo(devices) {
    this.deviceVideoPreviewResolutionMap_ = new Map();
    this.supportedResolutions_ = new Map();
    this.constFpsInfo_ = {};

    devices.forEach(({deviceId, videoResols, videoMaxFps, fpsRanges}) => {
      this.supportedResolutions_.set(
          deviceId, [...videoResols].sort((r1, r2) => r2.area - r1.area));

      // Filter out preview resolution greater than 1920x1080 and 1600x1200.
      const previewRatios = this.groupResolutionRatio_(videoResols.filter(
          ({width, height}) => width <= 1920 && height <= 1200));
      const videoRatios = this.groupResolutionRatio_(videoResols);
      /**
       * @type {!Array<{videoRs: !ResolutionList, previewRs:
       *     !ResolutionList}>}
       */
      const pairedResolutions = [];
      for (const [ratio, videoRs] of videoRatios) {
        const previewRs = previewRatios.get(/** @type {number} */ (ratio));
        if (previewRs === undefined) {
          continue;
        }
        pairedResolutions.push(
            {videoRs: /** @type {!ResolutionList} */ (videoRs), previewRs});
      }
      this.deviceVideoPreviewResolutionMap_.set(deviceId, pairedResolutions);

      /**
       * @param {number} width
       * @param {number} height
       * @return {!Resolution|undefined}
       */
      const findResol = (width, height) =>
          videoResols.find((r) => r.width === width && r.height === height);
      /** @type {!Resolution} */
      let prefR = this.getPrefResolution(deviceId) || findResol(1920, 1080) ||
          findResol(1280, 720) || new Resolution(0, -1);
      if (findResol(prefR.width, prefR.height) === undefined) {
        prefR = videoResols.reduce(
            (maxR, R) => (maxR.area < R.area ? R : maxR),
            new Resolution(0, -1));
      }
      this.prefResolution_.set(deviceId, prefR);

      const /** !Array<number> */ constFpses =
          fpsRanges.filter(({minFps, maxFps}) => minFps === maxFps)
              .map(({minFps}) => minFps);
      /** @type {!Object<string, !Array<number>>} */
      const fpsInfo = {};
      for (const [resolution, maxFps] of Object.entries(videoMaxFps)) {
        fpsInfo[resolution.toString()] =
            constFpses.filter((fps) => fps <= /** @type {number} */ (maxFps));
      }
      this.constFpsInfo_[deviceId] = fpsInfo;
    });
    this.saveResolutionPreference_('deviceVideoResolution');
  }

  /**
   * @override
   */
  updateValues(deviceId, stream, facing, resolution) {
    this.deviceId_ = deviceId;
    this.resolution_ = resolution;
    this.prefResolution_.set(deviceId, this.resolution_);
    this.saveResolutionPreference_('deviceVideoResolution');
    this.preferredResolutionChangeListener_(deviceId, this.resolution_);

    const fps = stream.getVideoTracks()[0].getSettings().frameRate;
    this.setPreferredConstFps_(deviceId, this.resolution_, fps);
    const supportedConstFpses =
        this.constFpsInfo_[deviceId][this.resolution_.toString()].filter(
            (fps) => SUPPORTED_CONSTANT_FPS.includes(fps));
    // Only enable multi fps UI on external camera.
    // See https://crbug.com/1059191 for details.
    state.set(
        state.State.MULTI_FPS,
        facing === Facing.EXTERNAL && supportedConstFpses.length > 1);
  }

  /**
   * @param {string} deviceId
   * @return {!Array<!CaptureCandidate>}
   * @private
   */
  getMultiStreamSortedCandidates_(deviceId) {
    /** @type {!Resolution} */
    const prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);

    /**
     * Maps specified video resolution to object of resolution and all supported
     * constant fps under that resolution or null fps for not support constant
     * fps. The resolution-fpses are sorted by user preference of constant fps.
     * @param {!Resolution} r
     * @return {!Array<{r: !Resolution, fps: number}>}
     */
    const getFpses = (r) => {
      let /** !Array<?number> */ constFpses = [null];
      /** @type {!Array<number>} */
      const constFpsInfo = this.constFpsInfo_[deviceId][r.toString()];
      // The higher constant fps will be ignored if constant 30 and 60 presented
      // due to currently lack of UI support for toggling it.
      if (constFpsInfo.includes(30) && constFpsInfo.includes(60)) {
        const prefFps = this.prefFpses_[deviceId] ?
            this.prefFpses_[deviceId][r.toString()] :
            30;
        constFpses = prefFps === 30 ? [30, 60] : [60, 30];
      } else {
        constFpses =
            [...constFpsInfo.filter((fps) => fps >= 30).sort().reverse(), null];
      }
      return constFpses.map((fps) => ({r, fps}));
    };

    /**
     * @param {{videoRs: !ResolutionList, previewRs: !ResolutionList}} capture
     * @return {!Array<!CaptureCandidate>}
     */
    const toVideoCandidate = ({videoRs, previewRs}) => {
      let /** !Resolution */ videoR = prefR;
      if (!videoRs.some((r) => r.equals(prefR))) {
        videoR = videoRs.reduce(
            (videoR, r) => (r.width > videoR.width ? r : videoR));
      }

      return getFpses(videoR).map(
          ({r, fps}) => ({
            resolution: videoR,
            previewCandidates:
                this.sortPreview_(previewRs, videoR)
                    .map(({width, height}) => ({
                           deviceId,
                           audio: true,
                           video: {
                             frameRate: fps ? {exact: fps} :
                                              {min: 20, ideal: 30},
                             width,
                             height,
                           },
                         })),
          }));
    };

    return this.deviceVideoPreviewResolutionMap_.get(deviceId)
        .flatMap(toVideoCandidate)
        .sort(this.getPreferResolutionSort_(prefR));
  }

  /**
   * @param {string} deviceId
   * @return {!Array<!CaptureCandidate>}
   * @private
   */
  getSortedCandidates_(deviceId) {
    /**
     * Maps specified video resolution to object of resolution and all supported
     * constant fps under that resolution or null fps for not support constant
     * fps. The resolution-fpses are sorted by user preference of constant fps.
     * @param {!Resolution} r
     * @return {!Array<{r: !Resolution, fps: number}>}
     */
    const getFpses = (r) => {
      let /** !Array<?number> */ constFpses = [null];
      /** @type {!Array<number>} */
      const constFpsInfo = this.constFpsInfo_[deviceId][r.toString()];
      // The higher constant fps will be ignored if constant 30 and 60 presented
      // due to currently lack of UI support for toggling it.
      if (constFpsInfo.includes(30) && constFpsInfo.includes(60)) {
        const prefFps = this.prefFpses_[deviceId] ?
            this.prefFpses_[deviceId][r.toString()] :
            30;
        constFpses = prefFps === 30 ? [30, 60] : [60, 30];
      } else {
        constFpses =
            [...constFpsInfo.filter((fps) => fps >= 30).sort().reverse(), null];
      }
      return constFpses.map((fps) => ({r, fps}));
    };

    /**
     * @param {!Resolution} r
     * @param {number} fps
     * @return {!StreamConstraints}
     */
    const toPreivewConstraints = ({width, height}, fps) => ({
      deviceId,
      audio: true,
      video: {
        frameRate: fps ? {exact: fps} : {min: 20, ideal: 30},
        width,
        height,
      },
    });

    const prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);
    return [...this.supportedResolutions_.get(deviceId)]
        .flatMap(getFpses)
        .map(({r, fps}) => ({
               resolution: r,
               // For non-multistream recording, preview stream is used directly
               // to do video recording.
               previewCandidates: [toPreivewConstraints(r, fps)],
             }))
        .sort(this.getPreferResolutionSort_(prefR));
  }

  /**
   * @override
   */
  getSortedCandidates(deviceId) {
    if (state.get(state.State.ENABLE_MULTISTREAM_RECORDING)) {
      return this.getMultiStreamSortedCandidates_(deviceId);
    }
    return this.getSortedCandidates_(deviceId);
  }
}

/**
 * Controller for handling photo resolution preference.
 */
export class PhotoConstraintsPreferrer extends ConstraintsPreferrer {
  /**
   * @param {function(): !Promise} doReconfigureStream
   * @public
   */
  constructor(doReconfigureStream) {
    super(doReconfigureStream);

    /**
     * Maps from device id as key to capture and preview resolutions of
     * same aspect ratio supported by that video device as value.
     * @type {!Map<string, !Array<{captureRs: !ResolutionList, previewRs:
     *     !ResolutionList}>>}
     * @private
     */
    this.deviceCapturePreviewResolutionMap_ = new Map();

    /**
     * Maps from device id as key to whether PTZ is support from device level.
     * @type {!Map<string, boolean>}
     */
    this.devicePTZSupportMap_ = new Map();

    this.restoreResolutionPreference_('devicePhotoResolution');
  }

  /**
   * @override
   */
  changePreferredResolution(deviceId, resolution) {
    this.prefResolution_.set(deviceId, resolution);
    this.saveResolutionPreference_('devicePhotoResolution');
    if (!state.get(Mode.VIDEO) && deviceId === this.deviceId_) {
      this.doReconfigureStream_();
    } else {
      this.preferredResolutionChangeListener_(deviceId, resolution);
    }
  }

  /**
   * @override
   */
  updateDevicesInfo(devices) {
    this.deviceCapturePreviewResolutionMap_ = new Map();
    this.supportedResolutions_ = new Map();
    this.devicePTZSupportMap_ = new Map(
        devices.map(({deviceId, supportPTZ}) => [deviceId, supportPTZ]));

    devices.forEach(({deviceId, photoResols, videoResols: previewResols}) => {
      const previewRatios = this.groupResolutionRatio_(previewResols);
      const captureRatios = this.groupResolutionRatio_(photoResols);
      /**
       * @type {!Array<{captureRs: !ResolutionList, previewRs:
       *     !ResolutionList}>}
       */
      const pairedResolutions = [];
      for (const [ratio, captureRs] of captureRatios) {
        const previewRs = previewRatios.get(/** @type {number} */ (ratio));
        if (previewRs === undefined) {
          continue;
        }
        pairedResolutions.push(
            {captureRs: /** @type {!ResolutionList} */ (captureRs), previewRs});
      }

      this.deviceCapturePreviewResolutionMap_.set(deviceId, pairedResolutions);
      this.supportedResolutions_.set(
          deviceId,
          pairedResolutions.flatMap(({captureRs}) => captureRs)
              .sort((r1, r2) => r2.area - r1.area));

      /** @type {!Resolution} */
      let prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);
      const captureRs = this.supportedResolutions_.get(deviceId);
      if (!captureRs.some((r) => r.equals(prefR))) {
        prefR = captureRs.reduce(
            (maxR, R) => (maxR.area < R.area ? R : maxR),
            new Resolution(0, -1));
      }
      this.prefResolution_.set(deviceId, prefR);
    });
    this.saveResolutionPreference_('devicePhotoResolution');
  }

  /**
   * @override
   */
  updateValues(deviceId, stream, facing, resolution) {
    this.deviceId_ = deviceId;
    this.prefResolution_.set(deviceId, resolution);
    this.saveResolutionPreference_('devicePhotoResolution');
    this.preferredResolutionChangeListener_(deviceId, resolution);
  }

  /**
   * @override
   */
  getSortedCandidates(deviceId) {
    /** @type {!Resolution} */
    const prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);
    const supportPTZ = this.devicePTZSupportMap_.get(deviceId) || false;

    /**
     * @param {{captureRs: !ResolutionList, previewRs: !ResolutionList}} capture
     * @return {!CaptureCandidate}
     */
    const toCaptureCandidate = ({captureRs, previewRs}) => {
      let /** !Resolution */ captureR = prefR;
      if (!captureRs.some((r) => r.equals(prefR))) {
        captureR = captureRs.reduce(
            (captureR, r) => (r.width > captureR.width ? r : captureR));
      }

      // Use workaround for b/184089334 on PTZ camera to use preview frame as
      // photo result.
      if (supportPTZ &&
          previewRs.find((r) => captureR.equals(r)) !== undefined) {
        previewRs = [captureR];
      }

      const /** !Array<!StreamConstraints> */ previewCandidates =
          this.sortPreview_(previewRs, captureR).map(({width, height}) => ({
                                                       deviceId,
                                                       audio: false,
                                                       video: {
                                                         width,
                                                         height,
                                                       },
                                                     }));
      return {resolution: captureR, previewCandidates};
    };

    return this.deviceCapturePreviewResolutionMap_.get(deviceId)
        .map(toCaptureCandidate)
        .sort(this.getPreferResolutionSort_(prefR));
  }
}
