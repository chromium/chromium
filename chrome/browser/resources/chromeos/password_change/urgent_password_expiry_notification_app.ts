// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'urgent-password-expiry-notification' is a notification that
 * warns the user their password is about to expire - but it is a large
 * notification that is shown in the center of the screen.
 * It is implemented not using the notification system, but as a system dialog.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '/strings.m.js';

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './urgent_password_expiry_notification_app.html.js';

const ONE_SECOND_IN_MS = 1000;
const ONE_MINUTE_IN_MS = ONE_SECOND_IN_MS * 60;
const ONE_HOUR_IN_MS = ONE_MINUTE_IN_MS * 60;

const UrgentPasswordExpiryNotificationElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

class UrgentPasswordExpiryNotificationElement extends
    UrgentPasswordExpiryNotificationElementBase {
  static get is() {
    return 'urgent-password-expiry-notification';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      title_: {
        type: String,
        value: '',
      },
    };
  }

  private title_: string;
  private expirationTimeMs_: number = 0;
  private countDownIntervalId_: number|null = null;
  private countDownIntervalMs_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();

    if (loadTimeData.valueExists('initialTitle')) {
      this.title_ = loadTimeData.getString('initialTitle');
    }

    if (loadTimeData.valueExists('expirationTime')) {
      const expirationTimeStr = loadTimeData.getString('expirationTime');
      const expirationTimeMs = parseInt(expirationTimeStr);
      if (isNaN(expirationTimeMs)) {
        console.error('Bad expiration time: ' + expirationTimeStr);
      } else {
        this.expirationTimeMs_ = expirationTimeMs;
        this.ensureCountDownCalledOftenEnough_();
      }
    }
  }

  private ensureCountDownCalledOftenEnough_() {
    const nowMs = Date.now();
    if (nowMs > this.expirationTimeMs_) {
      // Already expired - no need to keep updating UI.
      this.stopCountDownCalls_();
    } else if (nowMs >= (this.expirationTimeMs_ - 2 * ONE_HOUR_IN_MS)) {
      // Expires in the next two hours - update UI every minute.
      this.ensureCountDownCalledWithInterval_(ONE_MINUTE_IN_MS);
    } else {
      // Expires some time in the future - update UI every hour.
      this.ensureCountDownCalledWithInterval_(ONE_HOUR_IN_MS);
    }
  }

  private ensureCountDownCalledWithInterval_(intervalMs: number) {
    if (this.countDownIntervalMs_ === intervalMs) {
      return;
    }
    this.stopCountDownCalls_();
    this.countDownIntervalId_ =
        setInterval(this.countDown_.bind(this), intervalMs);
    this.countDownIntervalMs_ = intervalMs;
  }

  private stopCountDownCalls_() {
    if (!this.countDownIntervalId_) {
      return;
    }
    clearInterval(this.countDownIntervalId_);
    this.countDownIntervalId_ = null;
    this.countDownIntervalMs_ = null;
  }

  private countDown_() {
    this.ensureCountDownCalledOftenEnough_();
    const msUntilExpiry = this.expirationTimeMs_ - Date.now();
    sendWithPromise('getTitleText', msUntilExpiry).then((title) => {
      this.title_ = title;
    });
  }

  private onButtonTap_() {
    chrome.send('continue');
  }
}

customElements.define(
    UrgentPasswordExpiryNotificationElement.is,
    UrgentPasswordExpiryNotificationElement);
