// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for device.
 */
cca.device = cca.device || {};

/**
 * import {Resolution} from '../type.js';
 */
var Resolution = Resolution || {};

/* eslint-disable no-unused-vars */

/**
 * Candidate of capturing with specified photo or video resolution and
 * constraints-candidates it corresponding preview.
 * Video/photo capture resolution and the constraints-candidates of its
 * corresponding preview stream.
 * @typedef {{
 *   resolution: !Resolution,
 *   previewCandidates: !Array<!MediaStreamConstraints>
 * }}
 */
var CaptureCandidate;

/* eslint-enable no-unused-vars */

/**
 * Controller for managing preference of capture settings and generating a list
 * of stream constraints-candidates sorted by user preference.
 * @abstract
 */
cca.device.ConstraintsPreferrer = class {
  /**
   * @param {!function()} doReconfigureStream Trigger stream reconfiguration to
   *     reflect changes in user preferred settings.
   * @protected
   */
  constructor(doReconfigureStream) {
    /**
     * @type {!function()}
     * @protected
     */
    this.doReconfigureStream_ = doReconfigureStream;

    /**
     * Object saving resolution preference that each of its key as device id and
     * value to be preferred width, height of resolution of that video device.
     * @type {!Object<string, !Resolution>}
     * @protected
     */
    this.prefResolution_ = {};

    /**
     * Device id of currently working video device.
     * @type {?string}
     * @protected
     */
    this.deviceId_ = null;

    /**
     * Object of device id as its key and all of available capture resolutions
     * supported by that video device as its value.
     * @type {!Object<string, !ResolutionList>}
     * @protected
     */
    this.deviceResolutions_ = {};

    /**
     * Listener for changes of preferred resolution used on particular video
     * device.
     * @type {!function(string, !Resolution)}
     * @private
     */
    this.preferredResolutionChangeListener_ = () => {};
  }

  /**
   * Restores saved preferred capture resolution per video device.
   * @param {string} key Key of local storage saving preferences.
   * @protected
   */
  restoreResolutionPreference_(key) {
    // TODO(inker): Return promise and await it to assure preferences are loaded
    // before any access.
    cca.proxy.browserProxy.localStorageGet({[key]: {}}, (values) => {
      this.prefResolution_ = {};
      for (const [deviceId, {width, height}] of Object.entries(values[key])) {
        this.prefResolution_[deviceId] = new Resolution(width, height);
      }
    });
  }

  /**
   * Saves preferred capture resolution per video device.
   * @param {string} key Key of local storage saving preferences.
   * @protected
   */
  saveResolutionPreference_(key) {
    cca.proxy.browserProxy.localStorageSet({[key]: this.prefResolution_});
  }

  /**
   * Gets user preferred capture resolution for a specific device.
   * @param {string} deviceId Device id of the device.
   * @return {?Resolution} Returns preferred resolution or null if no preferred
   *     resolution found in user preference.
   */
  getPrefResolution(deviceId) {
    return this.prefResolution_[deviceId] || null;
  }

  /**
   * Updates with new video device information.
   * @param {!Array<!cca.device.Camera3DeviceInfo>} devices
   * @abstract
   */
  updateDevicesInfo(devices) {}

  /**
   * Updates values according to currently working video device and capture
   * settings.
   * @param {string} deviceId Device id of video device to be updated.
   * @param {!MediaStream} stream Currently active preview stream.
   * @param {!Resolution} resolution Resolution to be updated to.
   * @abstract
   */
  updateValues(deviceId, stream, resolution) {}

  /**
   * Gets all available candidates for capturing under this controller and its
   * corresponding preview constraints for the specified video device. Returned
   * resolutions and constraints candidates are both sorted in desired trying
   * order.
   * @abstract
   * @param {string} deviceId Device id of video device.
   * @param {!ResolutionList} previewResolutions Available preview resolutions
   *     for the video device.
   * @return {!Array<!CaptureCandidate>} Capture resolution and its preview
   *     constraints-candidates.
   */
  getSortedCandidates(deviceId, previewResolutions) {}

  /**
   * Changes user preferred capture resolution.
   * @abstract
   * @param {string} deviceId Device id of the video device to be changed.
   * @param {!Resolution} resolution Preferred capture resolution.
   */
  changePreferredResolution(deviceId, resolution) {}

  /**
   * Sets listener for changes of preferred resolution used in taking photo on
   * particular video device.
   * @param {!function(string, !Resolution)} listener
   */
  setPreferredResolutionChangeListener(listener) {
    this.preferredResolutionChangeListener_ = listener;
  }
};

/**
 * All supported constant fps options of video recording.
 * @type {!Array<number>}
 * @const
 */
cca.device.SUPPORTED_CONSTANT_FPS = [30, 60];

/**
 * Controller for handling video resolution preference.
 */
cca.device.VideoConstraintsPreferrer =
    class extends cca.device.ConstraintsPreferrer {
  /**
   * @param {!function()} doReconfigureStream
   * @public
   */
  constructor(doReconfigureStream) {
    super(doReconfigureStream);

    /**
     * Object saving information of device supported constant fps. Each of its
     * key as device id and value as an object mapping from resolution to all
     * constant fps options supported by that resolution.
     * @type {!Object<string, !Object<!Resolution, !Array<number>>>}
     * @private
     */
    this.constFpsInfo_ = {};

    /**
     * Object saving fps preference that each of its key as device id and value
     * as an object mapping from resolution to preferred constant fps for that
     * resolution.
     * @type {!Object<string, !Object<!Resolution, number>>}
     * @private
     */
    this.prefFpses_ = {};

    /**
     * @type {!HTMLButtonElement}
     * @const
     * @private
     */
    this.toggleFps_ = /** @type {!HTMLButtonElement} */ (
        document.querySelector('#toggle-fps'));

    /**
     * Currently in used recording resolution.
     * @type {!Resolution}
     * @protected
     */
    this.resolution_ = new Resolution(0, -1);
    this.restoreResolutionPreference_('deviceVideoResolution');
    this.restoreFpsPreference_();

    this.toggleFps_.addEventListener('click', (event) => {
      if (!cca.state.get('streaming') || cca.state.get('taking')) {
        event.preventDefault();
      }
    });
    this.toggleFps_.addEventListener('change', (event) => {
      this.setPreferredConstFps_(
          /** @type {string} */ (this.deviceId_), this.resolution_,
          this.toggleFps_.checked ? 60 : 30);
      cca.state.set('mode-switching', true);
      this.doReconfigureStream_().finally(
          () => cca.state.set('mode-switching', false));
    });
  }

  /**
   * Restores saved preferred fps per video resolution per video device.
   * @private
   */
  restoreFpsPreference_() {
    cca.proxy.browserProxy.localStorageGet(
        {deviceVideoFps: {}},
        (values) => this.prefFpses_ = values.deviceVideoFps);
  }

  /**
   * Saves preferred fps per video resolution per video device.
   * @private
   */
  saveFpsPreference_() {
    cca.proxy.browserProxy.localStorageSet({deviceVideoFps: this.prefFpses_});
  }

  /**
   * @override
   */
  changePreferredResolution(deviceId, resolution) {
    this.prefResolution_[deviceId] = resolution;
    this.saveResolutionPreference_('deviceVideoResolution');
    if (cca.state.get('video-mode') && deviceId === this.deviceId_) {
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
    if (!cca.device.SUPPORTED_CONSTANT_FPS.includes(prefFps)) {
      return;
    }
    this.toggleFps_.checked = prefFps === 60;
    cca.device.SUPPORTED_CONSTANT_FPS.forEach(
        (fps) => cca.state.set(`_${fps}fps`, fps === prefFps));
    this.prefFpses_[deviceId] = this.prefFpses_[deviceId] || {};
    this.prefFpses_[deviceId][resolution] = prefFps;
    this.saveFpsPreference_();
  }

  /**
   * @override
   */
  updateDevicesInfo(devices) {
    this.deviceResolutions_ = {};
    this.constFpsInfo_ = {};

    devices.forEach(({deviceId, videoResols, videoMaxFps, fpsRanges}) => {
      this.deviceResolutions_[deviceId] = videoResols;
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
      this.prefResolution_[deviceId] = prefR;

      const /** !Array<number> */ constFpses =
          fpsRanges.filter(({minFps, maxFps}) => minFps === maxFps)
              .map(({minFps}) => minFps);
      const /** !Object<(!Resolution|string), !Array<number>> */ fpsInfo = {};
      for (const [resolution, maxFps] of Object.entries(videoMaxFps)) {
        fpsInfo[/** @type {string} */ (resolution)] =
            constFpses.filter((fps) => fps <= maxFps);
      }
      this.constFpsInfo_[deviceId] = fpsInfo;
    });
    this.saveResolutionPreference_('deviceVideoResolution');
  }

  /**
   * @override
   */
  updateValues(deviceId, stream, resolution) {
    this.deviceId_ = deviceId;
    this.resolution_ = resolution;
    this.prefResolution_[deviceId] = this.resolution_;
    this.saveResolutionPreference_('deviceVideoResolution');
    this.preferredResolutionChangeListener_(deviceId, this.resolution_);

    const fps = stream.getVideoTracks()[0].getSettings().frameRate;
    this.setPreferredConstFps_(deviceId, this.resolution_, fps);
    const supportedConstFpses =
        this.constFpsInfo_[deviceId][this.resolution_].filter(
            (fps) => cca.device.SUPPORTED_CONSTANT_FPS.includes(fps));
    cca.state.set('multi-fps', supportedConstFpses.length > 1);
  }

  /**
   * @override
   */
  getSortedCandidates(deviceId, previewResolutions) {
    // Due to the limitation of MediaStream API, preview stream is used directly
    // to do video recording.

    /** @type {!Resolution} */
    const prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);
    /**
     * @param {!Resolution} r1
     * @param {!Resolution} r2
     * @return {number}
     */
    const sortPrefResol = (r1, r2) => {
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

    /**
     * Maps specified video resolution to object of resolution and all supported
     * constant fps under that resolution or null fps for not support constant
     * fps. The resolution-fpses are sorted by user preference of constant fps.
     * @param {!Resolution} r
     * @return {!Array<!{r: !Resolution, fps: number}>}
     */
    const getFpses = (r) => {
      let /** !Array<?number> */ constFpses = [null];
      /** @type {!Array<number>} */
      const constFpsInfo = this.constFpsInfo_[deviceId][r];
      // The higher constant fps will be ignored if constant 30 and 60 presented
      // due to currently lack of UI support for toggling it.
      if (constFpsInfo.includes(30) && constFpsInfo.includes(60)) {
        const prefFps =
            this.prefFpses_[deviceId] && this.prefFpses_[deviceId][r] || 30;
        constFpses = prefFps === 30 ? [30, 60] : [60, 30];
      } else {
        constFpses =
            [...constFpsInfo.filter((fps) => fps >= 30).sort().reverse(), null];
      }
      return constFpses.map((fps) => ({r, fps}));
    };

    /**
     * @param {!Resolution} r
     * @param {!number} fps
     * @return {!MediaStreamConstraints}
     */
    const toConstraints = ({width, height}, fps) => ({
      audio: {echoCancellation: false},
      video: {
        deviceId: {exact: deviceId},
        frameRate: fps ? {exact: fps} : {min: 24},
        width,
        height,
      },
    });

    return [...this.deviceResolutions_[deviceId]]
        .sort(sortPrefResol)
        .flatMap(getFpses)
        .map(({r, fps}) => ({
               resolution: r,
               previewCandidates: [toConstraints(r, fps)],
             }));
  }
};


/**
 * Controller for handling photo resolution preference.
 */
cca.device.PhotoConstraintsPreferrer =
    class extends cca.device.ConstraintsPreferrer {
  /**
   * @param {!function()} doReconfigureStream
   * @public
   */
  constructor(doReconfigureStream) {
    super(doReconfigureStream);

    this.restoreResolutionPreference_('devicePhotoResolution');
  }

  /**
   * @override
   */
  changePreferredResolution(deviceId, resolution) {
    this.prefResolution_[deviceId] = resolution;
    this.saveResolutionPreference_('devicePhotoResolution');
    if (!cca.state.get('video-mode') && deviceId === this.deviceId_) {
      this.doReconfigureStream_();
    } else {
      this.preferredResolutionChangeListener_(deviceId, resolution);
    }
  }

  /**
   * @override
   */
  updateDevicesInfo(devices) {
    this.deviceResolutions_ = {};

    devices.forEach(({deviceId, photoResols}) => {
      this.deviceResolutions_[deviceId] = photoResols;
      /** @type {!Resolution} */
      let prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);
      if (!photoResols.some((r) => r.equals(prefR))) {
        prefR = photoResols.reduce(
            (maxR, R) => (maxR.area < R.area ? R : maxR),
            new Resolution(0, -1));
      }
      this.prefResolution_[deviceId] = prefR;
    });
    this.saveResolutionPreference_('devicePhotoResolution');
  }

  /**
   * @override
   */
  updateValues(deviceId, stream, resolution) {
    this.deviceId_ = deviceId;
    this.prefResolution_[deviceId] = resolution;
    this.saveResolutionPreference_('devicePhotoResolution');
    this.preferredResolutionChangeListener_(deviceId, resolution);
  }

  /**
   * Finds and pairs photo resolutions and preview resolutions with the same
   * aspect ratio.
   * @param {!ResolutionList} captureResolutions Available photo capturing
   *     resolutions.
   * @param {!ResolutionList} previewResolutions Available preview resolutions.
   * @return {!Array<!{capture: !ResolutionList, preview: !ResolutionList}>}
   *     Each item of returned array is a object of capture and preview
   *     resolutions of same aspect ratio.
   * @private
   */
  pairCapturePreviewResolutions_(captureResolutions, previewResolutions) {
    /** @type {!Object<string, !ResolutionList>} */
    const previewRatios = previewResolutions.reduce((rs, r) => {
      rs[r.aspectRatio] = rs[r.aspectRatio] || [];
      rs[r.aspectRatio].push(r);
      return rs;
    }, {});
    /** @type {!Object<string, !ResolutionList>} */
    const captureRatios = captureResolutions.reduce((rs, r) => {
      if (r.aspectRatio in previewRatios) {
        rs[r.aspectRatio] = rs[r.aspectRatio] || [];
        rs[r.aspectRatio].push(r);
      }
      return rs;
    }, {});
    return Object.entries(captureRatios)
        .map(([
               /** string */ aspectRatio,
               /** !Resolution */ capture,
             ]) => ({capture, preview: previewRatios[aspectRatio]}));
  }

  /**
   * @override
   */
  getSortedCandidates(deviceId, previewResolutions) {
    /** @type {!ResolutionList} */
    const photoResolutions = this.deviceResolutions_[deviceId];

    /** @type {!Resolution} */
    const prefR = this.getPrefResolution(deviceId) || new Resolution(0, -1);

    /**
     * @param {!CaptureCandidate} candidate
     * @param {!CaptureCandidate} candidate2
     * @return {number}
     */
    const sortPrefResol = ({resolution: r1}, {resolution: r2}) => {
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
      return r2.area - r1.area;
    };

    /**
     * @param {!{capture: !ResolutionList, preview: !ResolutionList}} capture
     * @return {!CaptureCandidate}
     */
    const toCaptureCandidate = ({capture: captureRs, preview: previewRs}) => {
      let /** !Resolution */ captureR = prefR;
      if (!captureRs.some((r) => r.equals(prefR))) {
        captureR = captureRs.reduce(
            (captureR, r) => (r.width > captureR.width ? r : captureR));
      }

      /**
       * @param {!ResolutionList} rs
       * @return {!ResolutionList}
       */
      const sortPreview = (rs) => {
        if (rs.length === 0) {
          return [];
        }
        rs = [...rs].sort((r1, r2) => r2.width - r1.width);

        // Promote resolution slightly larger than screen size to the first.
        const /** number */ screenW =
            Math.floor(window.screen.width * window.devicePixelRatio);
        const /** number */ screenH =
            Math.floor(window.screen.height * window.devicePixelRatio);
        let /** ?number */ minIdx = null;
        rs.forEach(({width, height}, idx) => {
          if (width >= screenW && height >= screenH) {
            minIdx = idx;
          }
        });
        if (minIdx !== null) {
          rs.unshift(...rs.splice(minIdx, 1));
        }

        return rs;
      };

      const /** !Array<!MediaStreamConstraints> */ previewCandidates =
          sortPreview(previewRs).map(({width, height}) => ({
                                       audio: false,
                                       video: {
                                         deviceId: {exact: deviceId},
                                         width,
                                         height,
                                       },
                                     }));
      return {resolution: captureR, previewCandidates};
    };

    return this
        .pairCapturePreviewResolutions_(photoResolutions, previewResolutions)
        .map(toCaptureCandidate)
        .sort(sortPrefResol);
  }
};
