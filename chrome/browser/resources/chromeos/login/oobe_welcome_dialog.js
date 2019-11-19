// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  const LONG_TOUCH_TIME_MS = 1000;

  function TitleLongTouchDetector(element, callback) {
    this.callback_ = callback;

    element.addEventListener('touchstart', this.onTouchStart_.bind(this));
    element.addEventListener('touchend', this.killTimer_.bind(this));
    element.addEventListener('touchcancel', this.killTimer_.bind(this));

    element.addEventListener('mousedown', this.onTouchStart_.bind(this));
    element.addEventListener('mouseup', this.killTimer_.bind(this));
    element.addEventListener('mouseleave', this.killTimer_.bind(this));
  }

  TitleLongTouchDetector.prototype = {
    /**
     * This is timeout ID used to kill window timeout that fires "detected"
     * callback if touch event was cancelled.
     *
     * @private {number|null}
     */
    timeoutId_: null,

    /**
     *  window.setTimeout() callback.
     *
     * @private
     */
    onTimeout_: function() {
      this.killTimer_();
      this.callback_();
    },

    /**
     * @private
     */
    onTouchStart_: function() {
      this.killTimer_();
      this.timeoutId_ = window.setTimeout(
          this.onTimeout_.bind(this, this.attempt_), LONG_TOUCH_TIME_MS);
    },

    /**
     * @private
     */
    killTimer_: function() {
      if (this.timeoutId_ === null)
        return;

      window.clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    },
  };

  const VIDEO_DEVICE = {
    CHROMEBOX: 'chromebox',
    LAPTOP: 'laptop',
    TABLET: 'tablet',
    LAPTOP_G: 'laptop_G',
    TABLET_G: 'tablet_G',
  };

  const VIDEO_ORIENTATION = {
    PORTRAIT: 'portrait',
    LANDSCAPE: 'landscape',
  };

  const VIDEO_TYPE = {
    INTRO: 'intro',
    LOOP: 'loop',
  };

  class WelcomeVideoController {
    /**
     * This calculates key that is used to identify <video> node in internal
     * mapping between possible video configurations and actual videos.
     *
     * @param {VIDEO_DEVICE} device Allowed device type.
     * @param {VIDEO_ORIENTATION} orientation Allowed orientation.
     * @param {VIDEO_TYPE} type Allowed type.
     * @return {String} Returns <video> key to be stored in this.videos.
     * @private
     */
    calcKey_(device, orientation, type) {
      return device + '-' + orientation + '-' + type;
    }

    /**
     * @param {VIDEO_DEVICE} device Allowed device type.
     * @param {VIDEO_ORIENTATION} orientation Allowed orientation.
     */
    constructor(device, orientation) {
      this.devices = new Set();
      this.orientations = new Set();
      this.types = new Set();
      for (let key in VIDEO_DEVICE)
        this.devices.add(VIDEO_DEVICE[key]);

      for (let key in VIDEO_ORIENTATION)
        this.orientations.add(VIDEO_ORIENTATION[key]);

      for (let key in VIDEO_TYPE)
        this.types.add(VIDEO_TYPE[key]);

      assert(this.devices.has(device));
      assert(this.orientations.has(orientation));

      this.device = device;
      this.orientation = orientation;
      this.type = VIDEO_TYPE.INTRO;

      // This is map from 'device-orientation-video_type' to video DOM object.
      this.videos = new Map();
    }

    /**
     * Returns currently playing video.
     * If none are playing, falls back to currently visible one, because
     * it might have accidentally stopped because of media error.
     *
     * @return {Object|null} Returns <video> DOM node.
     * @private
     */
    getActiveVideo_() {
      for (let node of this.videos.values()) {
        if (!node.paused)
          return node;
      }
      // Media decode error stops video play. Try 'hidden' instead.
      for (let node of this.videos.values()) {
        if (!node.hasAttribute('hidden'))
          return node;
      }
      return null;
    }

    /**
     * @param {Object} video <video> DOM node with valid |oobe_devices|,
     *     |oobe_orientations| and |oobe_types|.
     * |oobe_devices| is a comma-separated list of VIDEO_DEVICE values.
     * |oobe_orientations| is a comma-separated list of VIDEO_ORIENTATION
     *     values.
     * |oobe_types| is a comma-separated list of VIDEO_TYPE values.
     */
    add(video) {
      // Split strings by comma and trim space.
      let devices = video.getAttribute('oobe_devices')
                        .split(',')
                        .map((str) => str.replace(/\s/g, ''));
      let orientations = video.getAttribute('oobe_orientations')
                             .split(',')
                             .map((str) => str.replace(/\s/g, ''));
      let types = video.getAttribute('oobe_types')
                      .split(',')
                      .map((str) => str.replace(/\s/g, ''));

      for (let device of devices) {
        assert(this.devices.has(device));

        for (let orientation of orientations) {
          assert(this.orientations.has(orientation));

          for (let type of types) {
            assert(this.types.has(type));

            let key = this.calcKey_(device, orientation, type);
            assert(!this.videos.has(key));

            this.videos.set(key, video);

            if (type == VIDEO_TYPE.INTRO)
              video.addEventListener('ended', (event) => {
                event.target.setAttribute('hidden', '');
                this.type = VIDEO_TYPE.LOOP;
                this.play();
              });
          }
        }
      }
    }

    /**
     * Hides all videos.
     * @private
     */
    hideAll_() {
      for (let node of this.videos.values())
        node.setAttribute('hidden', '');
    }

    /**
     * Updates screen configuration, and switches to appropriate video to match
     * it.
     *
     * @param {String} device Allowed device type.
     * @param {String} orientation Allowed orientation.
     */
    updateConfiguration(device, orientation) {
      assert(this.devices.has(device));
      assert(this.orientations.has(orientation));

      if (device === this.device && orientation === this.orientation)
        return;

      this.device = device;
      this.orientation = orientation;
      let previousVideo = this.getActiveVideo_();
      let nextKey = this.calcKey_(device, orientation, this.type);
      let nextVideo = this.videos.get(nextKey);

      // Some video may match more than one key.
      if (previousVideo === nextVideo)
        return;

      if (previousVideo)
        previousVideo.pause();

      if (previousVideo && nextVideo)
        nextVideo.currentTime = previousVideo.currentTime;

      this.hideAll_();
      if (nextVideo) {
        nextVideo.removeAttribute('hidden');
        nextVideo.play();
      }
    }

    /**
     *  Starts active video.
     */
    play() {
      if (this.getActiveVideo_())
        return;

      let key = this.calcKey_(this.device, this.orientation, this.type);
      let video = this.videos.get(key);

      if (video) {
        video.removeAttribute('hidden');
        video.play();
      }
    }
  }

  Polymer({
    is: 'oobe-welcome-dialog',

    behaviors: [I18nBehavior, OobeDialogHostBehavior],

    properties: {
      /**
       * Currently selected system language (display name).
       */
      currentLanguage: {
        type: String,
        value: '',
      },

      /**
       * Controls visibility of "Timezone" button.
       */
      timezoneButtonVisible: {
        type: Boolean,
        value: false,
        observer: 'updateVideoMode_',
      },

      /**
       * Controls displaying of "Enable debugging features" link.
       */
      debuggingLinkVisible: Boolean,

      /**
       * True when in tablet mode (vs laptop).
       */
      isInTabletMode: {
        type: Boolean,
        observer: 'updateVideoMode_',
      },

      /**
       * True when screen orientation is portrait (vs landscape).
       */
      isInPortraitMode: {
        type: Boolean,
        observer: 'updateVideoMode_',
      }
    },

    getVideoDeviceType_: function() {
      if (this.timezoneButtonVisible)
        return VIDEO_DEVICE.CHROMEBOX;

      return this.isInTabletMode ? VIDEO_DEVICE.TABLET : VIDEO_DEVICE.LAPTOP;
    },

    getVideoOrientationType_: function() {
      return this.isInPortraitMode ? VIDEO_ORIENTATION.PORTRAIT :
                                     VIDEO_ORIENTATION.LANDSCAPE;
    },

    updateVideoMode_: function() {
      // Depending on the order of events, this might be called before
      // attached().
      if (!this.welcomeVideoController_)
        return;

      this.welcomeVideoController_.updateConfiguration(
          this.getVideoDeviceType_(), this.getVideoOrientationType_());
    },

    /**
     * @private {TitleLongTouchDetector}
     */
    titleLongTouchDetector_: null,

    /**
     * @private {WelcomeVideoController}
     */
    welcomeVideoController_: null,

    /**
     * This is stored ID of currently focused element to restore id on returns
     * to this dialog from Language / Timezone Selection dialogs.
     */
    focusedElement_: 'languageSelectionButton',

    onLanguageClicked_: function() {
      this.focusedElement_ = 'languageSelectionButton';
      this.fire('language-button-clicked');
    },

    onAccessibilityClicked_: function() {
      this.focusedElement_ = 'accessibilitySettingsButton';
      this.fire('accessibility-button-clicked');
    },

    onTimezoneClicked_: function() {
      this.focusedElement_ = 'timezoneSettingsButton';
      this.fire('timezone-button-clicked');
    },

    onNextClicked_: function() {
      this.focusedElement_ = 'welcomeNextButton';
      this.fire('next-button-clicked');
    },

    onDebuggingLinkClicked_: function() {
      this.fire('enable-debugging-clicked');
    },

    /*
     * This is called from titleLongTouchDetector_ when long touch is detected.
     *
     * @private
     */
    onTitleLongTouch_: function() {
      this.fire('launch-advanced-options');
    },

    attached: function() {
      this.welcomeVideoController_ = new WelcomeVideoController(
          this.getVideoDeviceType_(), this.getVideoOrientationType_());
      let videos = Polymer.dom(this.root).querySelectorAll('video');
      for (let video of videos)
        this.welcomeVideoController_.add(video);

      this.titleLongTouchDetector_ = new TitleLongTouchDetector(
          this.$.title, this.onTitleLongTouch_.bind(this));
      this.focus();
    },

    focus: function() {
      this.onWindowResize();
      let focusedElement = this.$[this.focusedElement_];
      if (focusedElement)
        focusedElement.focus();
    },

    /**
     * This is called from oobe_welcome when this dialog is shown.
     */
    show: function() {
      this.focus();
      this.welcomeVideoController_.play();
    },

    /**
     * This function formats message for labels.
     * @param String label i18n string ID.
     * @param String parameter i18n string parameter.
     * @private
     */
    formatMessage_: function(label, parameter) {
      return loadTimeData.getStringF(label, parameter);
    },

    /**
     * Window-resize event listener (delivered through the display_manager).
     */
    onWindowResize: function() {
      this.isInPortraitMode = window.innerHeight > window.innerWidth;
    },
  });
}
