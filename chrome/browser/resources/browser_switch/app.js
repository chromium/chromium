// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserSwitchProxyImpl} from './browser_switch_proxy.js';

/** @type {number} */
const MS_PER_SECOND = 1000;

/** @enum {string} */
const LaunchError = {
  GENERIC_ERROR: 'genericError',
  PROTOCOL_ERROR: 'protocolError',
};

Polymer({
  is: 'browser-switch-app',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * URL to launch in the alternative browser.
     * @private
     */
    url_: {
      type: String,
      value: function() {
        return (new URLSearchParams(window.location.search)).get('url') || '';
      },
    },

    /**
     * Error message, or empty string if no error has occurred (yet).
     * @private
     */
    error_: {
      type: String,
      value: '',
    },

    /**
     * Countdown displayed to the user, number of seconds until launching. If 0
     * or less, doesn't get displayed at all.
     * @private
     */
    secondCounter_: {
      type: Number,
      value: 0,
    },
  },

  /** @override */
  attached: function() {
    // If '?done=...' is specified in the URL, this tab was-reopened, or the
    // entire browser was closed by LBS and re-opened. In that case, go to NTP
    // instead.
    const done = (new URLSearchParams(window.location.search)).has('done');
    if (done) {
      getProxy().gotoNewTabPage();
      return;
    }

    // Sanity check the URL to make sure nothing really funky is going on.
    const anchor = document.createElement('a');
    anchor.href = this.url_;
    if (!/^(file|http|https):$/.test(anchor.protocol)) {
      this.error_ = LaunchError.PROTOCOL_ERROR;
      return;
    }

    const milliseconds = loadTimeData.getInteger('launchDelay');
    setTimeout(this.launchAndCloseTab_.bind(this), milliseconds);
    this.startCountdown_(Math.floor(milliseconds / 1000));
  },

  /** @private */
  launchAndCloseTab_: function() {
    // Mark this page with '?done=...' so that restoring the tab doesn't
    // immediately re-trigger LBS.
    history.pushState({}, '', '/?done=true');

    getProxy().launchAlternativeBrowserAndCloseTab(this.url_).catch(() => {
      this.error_ = LaunchError.GENERIC_ERROR;
    });
  },

  /**
   * @param {number} seconds
   * @private
   */
  startCountdown_: function(seconds) {
    this.secondCounter_ = seconds;
    const intervalId = setInterval(() => {
      this.secondCounter_--;
      if (this.secondCounter_ <= 0) {
        clearInterval(intervalId);
      }
    }, 1 * MS_PER_SECOND);
  },

  /**
   * @return {string}
   * @private
   */
  computeTitle_: function() {
    if (this.error_) {
      return this.i18n('errorTitle', getBrowserName());
    }
    if (this.secondCounter_ > 0) {
      return this.i18n('countdownTitle', this.secondCounter_, getBrowserName());
    }
    return this.i18n('openingTitle', getBrowserName());
  },

  /**
   * @return {string}
   * @private
   */
  computeDescription_: function() {
    if (this.error_) {
      return this.i18n(
          this.error_, getUrlHostname(this.url_), getBrowserName());
    }
    return this.i18n(
        'description', getUrlHostname(this.url_), getBrowserName());
  },
});

function getBrowserName() {
  return loadTimeData.getString('browserName');
}

function getUrlHostname(url) {
  const anchor = document.createElement('a');
  anchor.href = url;
  // Return entire url if parsing failed (which means the URL is bogus).
  return anchor.hostname || url;
}

function getProxy() {
  return BrowserSwitchProxyImpl.getInstance();
}
