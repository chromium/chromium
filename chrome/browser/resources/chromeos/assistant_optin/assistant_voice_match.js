// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Maximum recording index. */
const MAX_INDEX = 4;

/**
 * Name of the screen.
 * @type {string}
 */
const VOICE_MATCH_SCREEN_ID = 'VoiceMatchScreen';

const VoiceMatchUIState = {
  INTRO: 'intro',
  RECORDING: 'recording',
  COMPLETED: 'completed',
  ALREADY_SETUP: 'already-setup',
};

/**
 * @fileoverview Polymer element for displaying material design assistant
 * voice match screen.
 *
 */
Polymer({
  is: 'assistant-voice-match',

  behaviors: [OobeI18nBehavior, MultiStepBehavior],

  /**
   * Whether voice match is the first screen of the flow.
   * @type {boolean}
   */
  isFirstScreen: false,

  /**
   * Current recording index.
   * @type {number}
   * @private
   */
  currentIndex_: 0,

  /**
   * The delay in ms between speaker ID enrollment finishes and the
   * voice-match-done action is reported to chrome.
   * @private {number}
   */
  doneActionDelayMs_: 3000,

  /** @private {?assistant.BrowserProxy} */
  browserProxy_: null,

  defaultUIStep() {
    return VoiceMatchUIState.INTRO;
  },

  UI_STEPS: VoiceMatchUIState,

  /**
   * Overrides the default delay for sending voice-match-done action.
   * @param {number} delay The delay to be used in tests.
   */
  setDoneActionDelayForTesting(delay) {
    this.doneActionDelayMs_ = delay;
  },

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_() {
    this.$['voice-match-lottie'].setPlay(false);
    this.browserProxy_.userActed(VOICE_MATCH_SCREEN_ID, ['skip-pressed']);
  },

  /**
   * On-tap event handler for agree button.
   *
   * @private
   */
  onAgreeTap_() {
    this.setUIStep(VoiceMatchUIState.RECORDING);
    this.fire('loading');
    this.browserProxy_.userActed(VOICE_MATCH_SCREEN_ID, ['record-pressed']);
  },

  /**
   * Reset the status of page elements.
   *
   * @private
   */
  resetElements_() {
    this.currentIndex_ = 0;

    this.$['voice-match-entries'].hidden = false;
    this.$['later-button'].hidden = false;
    this.$['loading-animation'].hidden = true;

    for (var i = 0; i < MAX_INDEX; ++i) {
      var entry = this.$['voice-entry-' + i];
      entry.removeAttribute('active');
      entry.removeAttribute('completed');
    }
  },

  /** @override */
  created() {
    this.browserProxy_ = assistant.BrowserProxyImpl.getInstance();
  },

  /**
   * Reloads voice match flow.
   */
  reloadPage() {
    this.setUIStep(VoiceMatchUIState.INTRO);
    this.$['agree-button'].focus();
    this.resetElements_();
    this.browserProxy_.userActed(VOICE_MATCH_SCREEN_ID, ['reload-requested']);
    this.fire('loaded');
  },

  /**
   * Called when the server is ready to listening for hotword.
   */
  listenForHotword() {
    if (this.currentIndex_ == 0) {
      this.fire('loaded');
      announceAccessibleMessage(
          loadTimeData.getString('assistantVoiceMatchRecording'));
      announceAccessibleMessage(
          loadTimeData.getString('assistantVoiceMatchA11yMessage'));
    }
    var currentEntry = this.$['voice-entry-' + this.currentIndex_];
    currentEntry.setAttribute('active', true);
  },

  /**
   * Called when the server has detected and processing hotword.
   */
  processingHotword() {
    var currentEntry = this.$['voice-entry-' + this.currentIndex_];
    currentEntry.removeAttribute('active');
    currentEntry.setAttribute('completed', true);
    this.currentIndex_++;
    if (this.currentIndex_ == MAX_INDEX) {
      this.$['voice-match-entries'].hidden = true;
      this.$['later-button'].hidden = true;
      this.$['loading-animation'].hidden = false;
      announceAccessibleMessage(
          loadTimeData.getString('assistantVoiceMatchUploading'));
    } else {
      announceAccessibleMessage(
          loadTimeData.getString('assistantVoiceMatchComplete'));
    }
  },

  voiceMatchDone() {
    this.fire('loaded');
    announceAccessibleMessage(
        loadTimeData.getString('assistantVoiceMatchCompleted'));
    if (this.currentIndex_ != MAX_INDEX) {
      // Existing voice model found on cloud. No need to train.
      this.$['later-button'].hidden = true;
      this.setUIStep(VoiceMatchUIState.ALREADY_SETUP);
    } else {
      this.setUIStep(VoiceMatchUIState.COMPLETED);
    }

    window.setTimeout(function() {
      this.$['voice-match-lottie'].setPlay(false);
      this.browserProxy_.userActed(VOICE_MATCH_SCREEN_ID, ['voice-match-done']);
    }.bind(this), this.doneActionDelayMs_);
  },

  /**
   * Signal from host to show the screen.
   */
  onShow() {
    if (this.isFirstScreen) {
      // If voice match is the first screen, slightly delay showing the content
      // for the lottie animations to load.
      this.fire('loading');
      window.setTimeout(function() {
        this.fire('loaded');
      }.bind(this), 100);
    }

    this.browserProxy_.screenShown(VOICE_MATCH_SCREEN_ID);
    this.$['voice-match-lottie'].setPlay(true);
    Polymer.RenderStatus.afterNextRender(
        this, () => this.$['agree-button'].focus());
  },

  /**
   * Returns the text for subtitle.
   */
  getSubtitleMessage_(locale) {
    return this.i18nAdvanced('assistantVoiceMatchMessage');
  },
});
