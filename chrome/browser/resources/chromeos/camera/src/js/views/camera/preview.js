// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Creates a controller for the video preview of Camera view.
 * @param {function()} onNewStreamNeeded Callback to request new stream.
 * @constructor
 */
cca.views.camera.Preview = function(onNewStreamNeeded) {
  /**
   * @type {function()}
   * @private
   */
  this.onNewStreamNeeded_ = onNewStreamNeeded;

  /**
   * Video element to capture the stream.
   * @type {Video}
   * @private
   */
  this.video_ = document.querySelector('#preview-video');

  /**
   * Element that shows the preview metadata.
   * @type {HTMLElement}
   * @private
   */
  this.metadata_ = document.querySelector('#preview-metadata');

  /**
   * The observer id for preview metadata.
   * @type {?number}
   * @private
   */
  this.metadataObserverId_ = null;

  /**
   * Current active stream.
   * @type {MediaStream}
   * @private
   */
  this.stream_ = null;

  /**
   * Watchdog for stream-end.
   * @type {?number}
   * @private
   */
  this.watchdog_ = null;

  /**
   * Promise for the current applying focus.
   * @type {?Promise}
   * @private
   */
  this.focus_ = null;

  /**
   * Timeout for resizing the window.
   * @type {?number}
   * @private
   */
  this.resizeWindowTimeout_ = null;

  /**
   * Aspect ratio for the window.
   * @type {number}
   * @private
   */
  this.aspectRatio_ = 1;

  // End of properties, seal the object.
  Object.seal(this);

  var inner = chrome.app.window.current().innerBounds;
  this.aspectRatio_ = inner.width / inner.height;
  window.addEventListener('resize', this.onWindowResize_.bind(this, null));

  ['expert', 'show-metadata'].forEach((state) => {
    cca.state.addObserver(state, this.updateShowMetadata_.bind(this));
  });

  this.video_.cleanup = () => {};
};

cca.views.camera.Preview.prototype = {
  get stream() {
    return this.stream_;
  },

  /**
   * @return {!HTMLVideoElement}
   */
  get video() {
    return this.video_;
  },
};

/**
 * @override
 */
cca.views.camera.Preview.prototype.toString = function() {
  return this.video_.videoHeight ?
      (this.video_.videoWidth + ' x ' + this.video_.videoHeight) : '';
};

/**
 * Sets video element's source.
 * @param {MediaStream} stream Stream to be the source.
 * @return {!Promise} Promise for the operation.
 */
cca.views.camera.Preview.prototype.setSource_ = function(stream) {
  var video = document.createElement('video');
  video.id = 'preview-video';
  video.classList = this.video_.classList;
  video.muted = true; // Mute to avoid echo from the captured audio.
  return new Promise((resolve) => {
    var handler = () => {
      video.removeEventListener('canplay', handler);
      resolve();
    };
    video.addEventListener('canplay', handler);
    video.srcObject = stream;
  }).then(() => video.play()).then(() => {
    video.cleanup = () => {
      video.removeAttribute('srcObject');
      video.load();
    };
    this.video_.parentElement.replaceChild(video, this.video_).cleanup();
    this.video_ = video;
    video.addEventListener('resize', () => this.onIntrinsicSizeChanged_());
    video.addEventListener('click', (event) => this.onFocusClicked_(event));
    return this.onIntrinsicSizeChanged_();
  });
};

/**
 * Starts the preview with the source stream.
 * @param {MediaStream} stream Stream to be the source.
 * @return {!Promise} Promise for the operation.
 */
cca.views.camera.Preview.prototype.start = function(stream) {
  return this.setSource_(stream).then(() => {
    // Use a watchdog since the stream.onended event is unreliable in the
    // recent version of Chrome. As of 55, the event is still broken.
    this.watchdog_ = setInterval(() => {
      // Check if video stream is ended (audio stream may still be live).
      if (!stream.getVideoTracks().length ||
          stream.getVideoTracks()[0].readyState == 'ended') {
        clearInterval(this.watchdog_);
        this.watchdog_ = null;
        this.stream_ = null;
        this.onNewStreamNeeded_();
      }
    }, 100);
    this.stream_ = stream;
    this.updateShowMetadata_();
    cca.state.set('streaming', true);
  });
};

/**
 * Stops the preview.
 */
cca.views.camera.Preview.prototype.stop = function() {
  if (this.watchdog_) {
    clearInterval(this.watchdog_);
    this.watchdog_ = null;
  }
  // Pause video element to avoid black frames during transition.
  this.video_.pause();
  if (this.stream_) {
    this.stream_.getVideoTracks()[0].stop();
    this.stream_ = null;
  }
  cca.state.set('streaming', false);
};

/**
 * Checks preview whether to show preview metadata or not.
 * @private
 */
cca.views.camera.Preview.prototype.updateShowMetadata_ = function() {
  if (cca.state.get('expert') && cca.state.get('show-metadata')) {
    this.enableShowMetadata_();
  } else {
    this.disableShowMetadata_();
  }
};

/**
 * Displays preview metadata on preview screen.
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.camera.Preview.prototype.enableShowMetadata_ = async function() {
  if (!this.stream_) {
    return;
  }

  document.querySelectorAll('.metadata-value').forEach((element) => {
    element.style.display = 'none';
  });

  const displayCategory = (selector, enabled) => {
    document.querySelector(selector).classList.toggle('mode-on', enabled);
  };

  const showValue = (selector, val) => {
    const element = document.querySelector(selector);
    element.style.display = '';
    element.textContent = val;
  };

  const buildInverseTable = (obj, prefix) => {
    const tbl = {};
    for (const [key, val] of Object.entries(obj)) {
      if (!key.startsWith(prefix)) {
        continue;
      }
      if (tbl.hasOwnProperty(val)) {
        console.error(`Duplicated value: ${val}`);
        continue;
      }
      tbl[val] = key.slice(prefix.length);
    }
    return tbl;
  };

  const afStateName = buildInverseTable(
      cros.mojom.AndroidControlAfState, 'ANDROID_CONTROL_AF_STATE_');
  const aeStateName = buildInverseTable(
      cros.mojom.AndroidControlAeState, 'ANDROID_CONTROL_AE_STATE_');
  const awbStateName = buildInverseTable(
      cros.mojom.AndroidControlAwbState, 'ANDROID_CONTROL_AWB_STATE_');
  const aeAntibandingModeName = buildInverseTable(
      cros.mojom.AndroidControlAeAntibandingMode,
      'ANDROID_CONTROL_AE_ANTIBANDING_MODE_');

  const tag = cros.mojom.CameraMetadataTag;
  const metadataEntryHandlers = {
    [tag.ANDROID_LENS_FOCUS_DISTANCE]: ([value]) => {
      if (value === 0) {
        // Fixed-focus camera
        return;
      }
      const focusDistance = (100 / value).toFixed(1);
      showValue('#preview-focus-distance', `${focusDistance} cm`);
    },
    [tag.ANDROID_CONTROL_AF_STATE]: ([value]) => {
      showValue('#preview-af-state', afStateName[value]);
    },
    [tag.ANDROID_SENSOR_SENSITIVITY]: ([value]) => {
      const sensitivity = value;
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
      showValue('#preview-ae-antibanding-mode', aeAntibandingModeName[value]);
    },
    [tag.ANDROID_CONTROL_AE_STATE]: ([value]) => {
      showValue('#preview-ae-state', aeStateName[value]);
    },
    [tag.ANDROID_COLOR_CORRECTION_GAINS]: ([valueRed, , , valueBlue]) => {
      const wbGainRed = valueRed.toFixed(2);
      showValue('#preview-wb-gain-red', `${wbGainRed}x`);
      const wbGainBlue = valueBlue.toFixed(2);
      showValue('#preview-wb-gain-blue', `${wbGainBlue}x`);
    },
    [tag.ANDROID_CONTROL_AWB_STATE]: ([value]) => {
      showValue('#preview-awb-state', awbStateName[value]);
    },
    [tag.ANDROID_CONTROL_AF_MODE]: ([value]) => {
      displayCategory('#preview-af', value !== tag.CONTROL_AF_MODE_OFF);
    },
    [tag.ANDROID_CONTROL_AE_MODE]: ([value]) => {
      displayCategory('#preview-ae', value !== tag.CONTROL_AE_MODE_OFF);
    },
    [tag.ANDROID_CONTROL_AWB_MODE]: ([value]) => {
      displayCategory('#preview-awb', value !== tag.CONTROL_AWB_MODE_OFF);
    },
  };

  // Currently there is no easy way to calculate the fps of a video element.
  // Here we use the metadata events to calculate a reasonable approximation.
  const updateFps = (() => {
    const FPS_MEASURE_FRAMES = 100;
    const timestamps = [];
    return () => {
      const now = performance.now();
      timestamps.push(now);
      if (timestamps.length > FPS_MEASURE_FRAMES) {
        timestamps.shift();
      }
      if (timestamps.length === 1) {
        return null;
      }
      return (timestamps.length - 1) / (now - timestamps[0]) * 1000;
    };
  })();

  const callback = (metadata) => {
    const fps = updateFps();
    if (fps !== null) {
      showValue('#preview-fps', `${fps.toFixed(0)} FPS`);
    }
    for (const entry of metadata.entries) {
      if (entry.count === 0) {
        continue;
      }
      const handler = metadataEntryHandlers[entry.tag];
      if (handler === undefined) {
        continue;
      }
      handler(cca.mojo.parseMetadataData(entry));
    }
  };

  const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
  if (!deviceOperator) {
    return;
  }

  const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
  this.metadataObserverId_ = await deviceOperator.addMetadataObserver(
      deviceId, callback, cros.mojom.StreamType.PREVIEW_OUTPUT);
};

/**
 * Hide display preview metadata on preview screen.
 * @return {!Promise} Promise for the operation.
 * @private
 */
cca.views.camera.Preview.prototype.disableShowMetadata_ = async function() {
  if (!this.stream_ || this.metadataObserverId_ === null) {
    return;
  }

  const deviceOperator = await cca.mojo.DeviceOperator.getInstance();
  if (!deviceOperator) {
    return;
  }

  const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
  const isSuccess = await deviceOperator.removeMetadataObserver(
      deviceId, this.metadataObserverId_);
  if (!isSuccess) {
    console.error(`Failed to remove metadata observer with id: ${
        this.metadataObserverId_}`);
  }
  this.metadataObserverId_ = null;
};

/**
 * Sets CCA window inner bound to specified size.
 * @param {number} width Width and min width of new window inner bound.
 * @param {number} height Height and min height of new window inner bound.
 * @return {Promise} Promise which is resolved when size change actually happen.
 * @private
 */
cca.views.camera.Preview.prototype.setWindowSize_ = function(width, height) {
  return new Promise((resolve) => {
    const listener = () => {
      chrome.app.window.current().onBoundsChanged.removeListener(listener);
      resolve();
    };
    chrome.app.window.current().onBoundsChanged.addListener(listener);
    const inner = chrome.app.window.current().innerBounds;
    const prevW = inner.width;
    const prevH = inner.height;
    inner.minWidth = inner.width = width;
    inner.minHeight = inner.height = height;
    if (prevW == width && prevH == height) {
      listener();
    }
  });
};

/**
 * Handles resizing the window for preview's aspect ratio changes.
 * @param {number=} aspectRatio Aspect ratio changed.
 * @return {Promise}
 * @private
 */
cca.views.camera.Preview.prototype.onWindowResize_ = function(aspectRatio) {
  if (this.resizeWindowTimeout_) {
    clearTimeout(this.resizeWindowTimeout_);
    this.resizeWindowTimeout_ = null;
  }
  cca.nav.onWindowResized();

  // Resize window for changed preview's aspect ratio or restore window size by
  // the last known window's aspect ratio.
  return new Promise((resolve) => {
           if (aspectRatio) {
             this.aspectRatio_ = aspectRatio;
             resolve();
           } else {
             this.resizeWindowTimeout_ = setTimeout(() => {
               this.resizeWindowTimeout_ = null;
               resolve();
             }, 500);  // Delay further resizing for smooth UX.
           }
         })
      .then(() => {
        // Resize window by aspect ratio only if it's not maximized or
        // fullscreen.
        if (cca.util.isWindowFullSize()) {
          return;
        }
        const width = chrome.app.window.current().innerBounds.minWidth;
        const height = Math.round(width * 9 / 16);
        return this.setWindowSize_(width, height);
      });
};

/**
 * Handles changed intrinsic size (first loaded or orientation changes).
 * @async
 * @private
 */
cca.views.camera.Preview.prototype.onIntrinsicSizeChanged_ = async function() {
  if (this.video_.videoWidth && this.video_.videoHeight) {
    await this.onWindowResize_(
        this.video_.videoWidth / this.video_.videoHeight);
  }
  this.cancelFocus_();
};

/**
 * Handles clicking for focus.
 * @param {Event} event Click event.
 * @private
 */
cca.views.camera.Preview.prototype.onFocusClicked_ = function(event) {
  this.cancelFocus_();

  // Normalize to square space coordinates by W3C spec.
  const x = event.offsetX / this.video_.offsetWidth;
  const y = event.offsetY / this.video_.offsetHeight;
  const constraints = {advanced: [{pointsOfInterest: [{x, y}]}]};
  const track = this.video_.srcObject.getVideoTracks()[0];
  const focus = track.applyConstraints(constraints).then(() => {
    if (focus != this.focus_) {
      return; // Focus was cancelled.
    }
    const aim = document.querySelector('#preview-focus-aim');
    const clone = aim.cloneNode(true);
    clone.style.left = `${event.offsetX + this.video_.offsetLeft}px`;
    clone.style.top = `${event.offsetY + this.video_.offsetTop}px`;
    clone.hidden = false;
    aim.parentElement.replaceChild(clone, aim);
  }).catch(console.error);
  this.focus_ = focus;
};

/**
 * Cancels the current applying focus.
 * @private
 */
cca.views.camera.Preview.prototype.cancelFocus_ = function() {
  this.focus_ = null;
  document.querySelector('#preview-focus-aim').hidden = true;
};
