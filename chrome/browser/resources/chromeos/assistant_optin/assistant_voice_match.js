// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/** Maximum recording index. */
const MAX_INDEX = 4;

/**
 * @fileoverview Polymer element for displaying material design assistant
 * voice match screen.
 *
 */
Polymer({
  is: 'assistant-voice-match',

  behaviors: [OobeDialogHostBehavior],

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

  /**
   * Overrides the default delay for sending voice-match-done action.
   * @param {number} delay The delay to be used in tests.
   */
  setDoneActionDelayForTesting: function(delay) {
    this.doneActionDelayMs_ = delay;
  },

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_: function() {
    chrome.send(
        'login.AssistantOptInFlowScreen.VoiceMatchScreen.userActed',
        ['skip-pressed']);
    this.$['voice-match-lottie'].setPlay(false);
    this.$['already-setup-lottie'].setPlay(false);
  },

  /**
   * On-tap event handler for agree button.
   *
   * @private
   */
  onAgreeTap_: function() {
    this.removeClass_('intro');
    this.addClass_('recording');
    this.fire('loading');
    chrome.send(
        'login.AssistantOptInFlowScreen.VoiceMatchScreen.userActed',
        ['record-pressed']);
  },

  /**
   * Add class to the list of classes of root elements.
   * @param {string} className class to add
   *
   * @private
   */
  addClass_: function(className) {
    this.$['voice-match-dialog'].classList.add(className);
  },

  /**
   * Remove class to the list of classes of root elements.
   * @param {string} className class to remove
   *
   * @private
   */
  removeClass_: function(className) {
    this.$['voice-match-dialog'].classList.remove(className);
  },

  /**
   * Reloads voice match flow.
   */
  reloadPage: function() {
    this.removeClass_('recording');
    this.removeClass_('already-setup');
    this.removeClass_('completed');
    this.addClass_('intro');
    this.$['agree-button'].focus();
    this.fire('loaded');
  },

  /**
   * Called when the server is ready to listening for hotword.
   */
  listenForHotword: function() {
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
  processingHotword: function() {
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

  voiceMatchDone: function() {
    this.removeClass_('recording');
    this.fire('loaded');
    announceAccessibleMessage(
        loadTimeData.getString('assistantVoiceMatchCompleted'));
    if (this.currentIndex_ != MAX_INDEX) {
      // Existing voice model found on cloud. No need to train.
      this.$['later-button'].hidden = true;
      this.addClass_('already-setup');
    } else {
      this.addClass_('completed');
    }

    window.setTimeout(function() {
      chrome.send(
          'login.AssistantOptInFlowScreen.VoiceMatchScreen.userActed',
          ['voice-match-done']);
      this.$['voice-match-lottie'].setPlay(false);
      this.$['already-setup-lottie'].setPlay(false);
    }, this.doneActionDelayMs_);
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    chrome.send('login.AssistantOptInFlowScreen.VoiceMatchScreen.screenShown');
    this.$['voice-match-lottie'].setPlay(true);
    this.$['already-setup-lottie'].setPlay(true);
    this.$['agree-button'].focus();
    if (loadTimeData.getBoolean('hotwordDspAvailable')) {
      this.$['no-dsp-message'].hidden = true;
    }
  },
});
