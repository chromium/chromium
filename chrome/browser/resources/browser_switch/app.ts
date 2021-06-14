// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserSwitchProxy, BrowserSwitchProxyImpl} from './browser_switch_proxy.js';

const MS_PER_SECOND: number = 1000;

enum LaunchError {
  GENERIC_ERROR = 'genericError',
  PROTOCOL_ERROR = 'protocolError',
}

const BrowserSwitchAppElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

class BrowserSwitchAppElement extends BrowserSwitchAppElementBase {
  static get is() {
    return 'browser-switch-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * URL to launch in the alternative browser.
       */
      url_: {
        type: String,
        value() {
          return (new URLSearchParams(window.location.search)).get('url') || '';
        },
      },

      /**
       * Error message, or empty string if no error has occurred (yet).
       */
      error_: {
        type: String,
        value: '',
      },

      /**
       * Countdown displayed to the user, number of seconds until launching. If
       * 0 or less, doesn't get displayed at all.
       */
      secondCounter_: {
        type: Number,
        value: 0,
      },
    };
  }

  private url_: string;
  private error_: string;
  private secondCounter_: number;

  /** @override */
  connectedCallback() {
    super.connectedCallback();

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
  }

  private launchAndCloseTab_() {
    // Mark this page with '?done=...' so that restoring the tab doesn't
    // immediately re-trigger LBS.
    history.pushState({}, '', '/?done=true');

    getProxy().launchAlternativeBrowserAndCloseTab(this.url_).catch(() => {
      this.error_ = LaunchError.GENERIC_ERROR;
    });
  }

  private startCountdown_(seconds: number) {
    this.secondCounter_ = seconds;
    const intervalId = setInterval(() => {
      this.secondCounter_--;
      if (this.secondCounter_ <= 0) {
        clearInterval(intervalId);
      }
    }, 1 * MS_PER_SECOND);
  }

  private computeTitle_(): string {
    if (this.error_) {
      return this.i18n('errorTitle', getBrowserName());
    }
    if (this.secondCounter_ > 0) {
      return this.i18n('countdownTitle', this.secondCounter_, getBrowserName());
    }
    return this.i18n('openingTitle', getBrowserName());
  }

  private computeDescription_(): string {
    if (this.error_) {
      return this.i18n(
          this.error_, getUrlHostname(this.url_), getBrowserName());
    }
    return this.i18n(
        'description', getUrlHostname(this.url_), getBrowserName());
  }
}

customElements.define(BrowserSwitchAppElement.is, BrowserSwitchAppElement);

function getBrowserName(): string {
  return loadTimeData.getString('browserName');
}

function getUrlHostname(url: string): string {
  const anchor = document.createElement('a');
  anchor.href = url;
  // Return entire url if parsing failed (which means the URL is bogus).
  return anchor.hostname || url;
}

function getProxy(): BrowserSwitchProxy {
  return BrowserSwitchProxyImpl.getInstance();
}
