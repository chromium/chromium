// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  const LONG_TOUCH_TIME_MS = 1000;

  class TitleLongTouchDetector {
    constructor(element, callback) {
      this.callback_ = callback;
      /**
       * This is timeout ID used to kill window timeout that fires "detected"
       * callback if touch event was cancelled.
       *
       * @private {number|null}
       */
      this.timeoutId_ = null;

      element.addEventListener('touchstart', () => void this.onTouchStart_());
      element.addEventListener('touchend', () => void this.killTimer_());
      element.addEventListener('touchcancel', () => void this.killTimer_());

      element.addEventListener('mousedown', () => void this.onTouchStart_());
      element.addEventListener('mouseup', () => void this.killTimer_());
      element.addEventListener('mouseleave', () => void this.killTimer_());
    }

    /**
     *  window.setTimeout() callback.
     *
     * @private
     */
    onTimeout_() {
      this.killTimer_();
      this.callback_();
    }

    /**
     * @private
     */
    onTouchStart_() {
      this.killTimer_();
      this.timeoutId_ =
          window.setTimeout(() => void this.onTimeout_(), LONG_TOUCH_TIME_MS);
    }

    /**
     * @private
     */
    killTimer_() {
      if (this.timeoutId_ === null)
        return;

      window.clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }
  }

  Polymer({
    is: 'oobe-welcome-dialog',

    behaviors: [OobeI18nBehavior],

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
      },

      /**
       * Controls displaying of "Enable debugging features" link.
       */
      debuggingLinkVisible: Boolean,

      /**
       * Observer for when this screen is hidden, or shown.
       */
      hidden: {
        type: Boolean,
        observer: 'updateHidden_',
        reflectToAttribute: true,
      },

      isMeet_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('flowType') &&
              (loadTimeData.getString('flowType') == 'meet');
        },
        readOnly: true,
      },

      osInstallEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('osInstallEnabled') &&
              loadTimeData.getBoolean('osInstallEnabled');
        },
        readOnly: true,
      },
    },

    onBeforeShow() {
      document.documentElement.setAttribute('new-layout', '');
      this.setVideoPlay_(true);
    },

    /**
     * @private {TitleLongTouchDetector}
     */
    titleLongTouchDetector_: null,

    /**
     * This is stored ID of currently focused element to restore id on returns
     * to this dialog from Language / Timezone Selection dialogs.
     */
    focusedElement_: null,

    onLanguageClicked_(e) {
      this.focusedElement_ = 'languageSelectionButton';
      this.fire('language-button-clicked');
    },

    onAccessibilityClicked_() {
      this.focusedElement_ = 'accessibilitySettingsButton';
      this.fire('accessibility-button-clicked');
    },

    onTimezoneClicked_() {
      this.focusedElement_ = 'timezoneSettingsButton';
      this.fire('timezone-button-clicked');
    },

    onNextClicked_() {
      this.focusedElement_ = 'getStarted';
      this.fire('next-button-clicked');
    },

    onOsInstallClicked_() {
      this.fire('os-install-clicked');
    },

    onDebuggingLinkClicked_() {
      this.fire('enable-debugging-clicked');
    },

    /*
     * This is called from titleLongTouchDetector_ when long touch is detected.
     *
     * @private
     */
    onTitleLongTouch_() {
      this.fire('launch-advanced-options');
    },

    attached() {
      this.titleLongTouchDetector_ = new TitleLongTouchDetector(
          this.$.title, () => void this.onTitleLongTouch_());
      this.$.chromeVoxHint.addEventListener('keydown', (event) => {
        // When the ChromeVox hint dialog is open, allow users to press the
        // space bar to activate ChromeVox. This is intended to help first time
        // users easily activate ChromeVox.
        if (this.$.chromeVoxHint.open && event.key === ' ') {
          this.activateChromeVox_();
          event.preventDefault();
          event.stopPropagation();
        }
      });
      this.focus();
    },

    focus() {
      if (!this.focusedElement_) {
        this.focusedElement_ = 'getStarted';
      }
      let focusedElement = this.$[this.focusedElement_];
      if (focusedElement)
        focusedElement.focus();
    },

    /*
     * Observer method for changes to the hidden property.
     * This replaces the show() function, in this class.
     */
    updateHidden_(newValue, oldValue) {
      let visible = !newValue;
      if (visible) {
        this.focus();
      }

      this.setVideoPlay_(visible);
    },

    /**
     * Play or pause welcome video.
     * @param Boolean play - whether play or pause welcome video.
     * @private
     */
    setVideoPlay_(play) {
      if (this.isMeet_)
        return;
      this.$.welcomeAnimation.setPlay(play);
    },

    /**
     * This function formats message for labels.
     * @param String label i18n string ID.
     * @param String parameter i18n string parameter.
     * @private
     */
    formatMessage_(label, parameter) {
      return loadTimeData.getStringF(label, parameter);
    },

    // ChromeVox hint section.

    /**
     * Called to show the ChromeVox hint dialog.
     */
    showChromeVoxHint() {
      this.$.chromeVoxHint.showDialog();
      this.setVideoPlay_(false);
    },

    /**
     * Called to close the ChromeVox hint dialog.
     */
    closeChromeVoxHint() {
      this.setVideoPlay_(true);
      this.$.chromeVoxHint.hideDialog();
    },

    /**
     * Called when the 'Continue without ChromeVox' button is clicked.
     * @private
     */
    dismissChromeVoxHint_() {
      this.fire('chromevox-hint-dismissed');
      this.closeChromeVoxHint();
    },

    /** @private */
    activateChromeVox_() {
      this.closeChromeVoxHint();
      this.fire('chromevox-hint-accepted');
    }
  });
}
