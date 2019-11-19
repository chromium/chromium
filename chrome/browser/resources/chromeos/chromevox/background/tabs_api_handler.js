// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Accesses Chrome's tabs extension API and gives
 * feedback for events that happen in the "Chrome of Chrome".
 */

goog.provide('TabsApiHandler');

goog.require('AbstractEarcons');
goog.require('AbstractTts');
goog.require('BrailleInterface');
goog.require('ChromeVox');
goog.require('NavBraille');


/**
 * Class that adds listeners and handles events from the tabs API.
 * @constructor
 */
TabsApiHandler = function() {
  /** @type {function(string, Array<string>=)} @private */
  this.msg_ = Msgs.getMsg.bind(Msgs);
  /**
   * Tracks whether the active tab has finished loading.
   * @type {boolean}
   * @private
   */
  this.lastActiveTabLoaded_ = false;

  chrome.tabs.onCreated.addListener(this.onCreated.bind(this));
  chrome.tabs.onRemoved.addListener(this.onRemoved.bind(this));
  chrome.tabs.onActivated.addListener(this.onActivated.bind(this));
  chrome.tabs.onUpdated.addListener(this.onUpdated.bind(this));

  /**
   * @type {?number} The window.setInterval ID for checking the loading
   *     status of the current tab.
   * @private
   */
  this.pageLoadIntervalID_ = null;

  /**
   * @type {?number} The tab ID of the tab being polled because it's loading.
   * @private
   */
  this.pageLoadTabID_ = null;
};

/**
 * @type {boolean}
 */
TabsApiHandler.shouldOutputSpeechAndBraille = true;

TabsApiHandler.prototype = {
  /**
   * Handles chrome.tabs.onCreated.
   * @param {Object} tab
   */
  onCreated: function(tab) {
    if (TabsApiHandler.shouldOutputSpeechAndBraille) {
      ChromeVox.tts.speak(
          this.msg_('chrome_tab_created'), QueueMode.FLUSH,
          AbstractTts.PERSONALITY_ANNOUNCEMENT);
      ChromeVox.braille.write(
          NavBraille.fromText(this.msg_('chrome_tab_created')));
    }
    ChromeVox.earcons.playEarcon(Earcon.OBJECT_OPEN);
  },

  /**
   * Handles chrome.tabs.onRemoved.
   * @param {Object} tab
   */
  onRemoved: function(tab) {
    ChromeVox.earcons.playEarcon(Earcon.OBJECT_CLOSE);

    chrome.tabs.query({active: true}, function(tabs) {
      if (tabs.length == 0 && this.isPlayingPageLoadingSound_()) {
        ChromeVox.earcons.cancelEarcon(Earcon.PAGE_START_LOADING);
        this.cancelPageLoadTimer_();
      }
    }.bind(this));
  },

  /**
   * Handles chrome.tabs.onActivated.
   * @param {Object} activeInfo
   */
  onActivated: function(activeInfo) {
    this.updateLoadingSoundsWhenTabFocusChanges_(activeInfo.tabId);
    chrome.tabs.get(activeInfo.tabId, function(tab) {
      if (tab.status == 'loading') {
        return;
      }

      if (TabsApiHandler.shouldOutputSpeechAndBraille) {
        var title = tab.title ? tab.title : tab.url;
        ChromeVox.tts.speak(
            this.msg_('chrome_tab_selected', [title]), QueueMode.FLUSH,
            AbstractTts.PERSONALITY_ANNOUNCEMENT);
        ChromeVox.braille.write(
            NavBraille.fromText(this.msg_('chrome_tab_selected', [title])));
      }
      ChromeVox.earcons.playEarcon(Earcon.OBJECT_SELECT);
    }.bind(this));
  },

  /**
   * Called when a tab becomes active or focused.
   * @param {number} tabId the id of the tab that's now focused and active.
   * @private
   */
  updateLoadingSoundsWhenTabFocusChanges_: function(tabId) {
    chrome.tabs.get(tabId, function(tab) {
      this.lastActiveTabLoaded_ = tab.status == 'complete';
      if (tab.status == 'loading' && !this.isPlayingPageLoadingSound_()) {
        ChromeVox.earcons.playEarcon(Earcon.PAGE_START_LOADING);
        this.startPageLoadTimer_(tabId);
      } else {
        ChromeVox.earcons.cancelEarcon(Earcon.PAGE_START_LOADING);
        this.cancelPageLoadTimer_();
      }
    }.bind(this));
  },

  /**
   * Handles chrome.tabs.onUpdated.
   * @param {number} tabId
   * @param {Object} selectInfo
   */
  onUpdated: function(tabId, selectInfo) {
    chrome.tabs.get(tabId, function(tab) {
      if (!tab.active) {
        return;
      }
      if (tab.status == 'loading') {
        this.lastActiveTabLoaded_ = false;
        if (!this.isPlayingPageLoadingSound_()) {
          ChromeVox.earcons.playEarcon(Earcon.PAGE_START_LOADING);
          this.startPageLoadTimer_(tabId);
        }
      } else if (!this.lastActiveTabLoaded_) {
        this.lastActiveTabLoaded_ = true;
        ChromeVox.earcons.playEarcon(Earcon.PAGE_FINISH_LOADING);
        this.cancelPageLoadTimer_();
      }
    }.bind(this));
  },

  /**
   * The chrome.tabs API doesn't always fire an onUpdated event when a
   * page finishes loading, so we poll it.
   * @param {number} tabId The id of the tab to monitor.
   * @private
   */
  startPageLoadTimer_: function(tabId) {
    if (this.pageLoadIntervalID_) {
      if (tabId == this.pageLoadTabID_) {
        return;
      }
      this.cancelPageLoadTimer_();
    }

    this.pageLoadTabID_ = tabId;
    this.pageLoadIntervalID_ = window.setInterval(function() {
      if (this.pageLoadTabID_) {
        this.onUpdated(this.pageLoadTabID_, {});
      }
    }.bind(this), 1000);
  },

  /**
   * Cancel the page loading timer because the active tab is loaded.
   * @private
   */
  cancelPageLoadTimer_: function() {
    if (this.pageLoadIntervalID_) {
      window.clearInterval(this.pageLoadIntervalID_);
      this.pageLoadIntervalID_ = null;
      this.pageLoadTabID_ = null;
    }
  },

  /**
   * @return {boolean} True if the page loading sound is playing and our
   * page loading timer is active.
   */
  isPlayingPageLoadingSound_: function() {
    return this.pageLoadIntervalID_ != null;
  }
};
