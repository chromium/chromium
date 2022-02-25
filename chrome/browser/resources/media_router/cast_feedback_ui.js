// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
const FeedbackType = {
  BUG: 'Bug',
  FEATURE_REQUEST: 'FeatureRequest',
  MIRRORING_QUALITY: 'MirroringQuality',
  DISCOVERY: 'Discovery',
  OTHER: 'Other'
};

/**
 * Keep in sync with MediaRouterCastFeedbackEvent in enums.xml.
 * @enum {number}
 */
export const FeedbackEvent = {
  OPENED: 0,
  SENDING: 1,
  RESENDING: 2,
  SUCCEEDED: 3,
  FAILED: 4,
  MAX_VALUE: 4,
};

/**
 * See
 * https://docs.google.com/document/d/1c20VYdwpUPyBRQeAS0CMr6ahwWnb0s26gByomOwqDjk
 * @interface
 */
export class FeedbackUiBrowserProxy {
  /**
   * Records an event using Chrome Metrics.
   * @param {FeedbackEvent} event
   */
  recordEvent(event) {}

  /**
   * Proxy for chrome.feedbackPrivate.sendFeedback().
   * @param {chrome.feedbackPrivate.FeedbackInfo} info
   * @return {!Promise<chrome.feedbackPrivate.Status>}
   */
  sendFeedback(info) {}
}

/** @implements {FeedbackUiBrowserProxy} */
export class FeedbackUiBrowserProxyImpl {
  /** @override */
  recordEvent(event) {
    chrome.send(
        'metricsHandler:recordInHistogram',
        ['MediaRouter.Cast.Feedback.Event', event, FeedbackEvent.MAX_VALUE]);
  }

  /** @override */
  sendFeedback(info) {
    return new Promise(
        resolve => chrome.feedbackPrivate.sendFeedback(
            info, /*loadSystemInfo=*/ null, /*formOpenTime=*/ null, resolve));
  }
}

addSingletonGetter(FeedbackUiBrowserProxyImpl);

export class FeedbackUiElement extends PolymerElement {
  constructor() {
    super();

    /** @private {FeedbackUiBrowserProxy} */
    this.browserProxy_ = FeedbackUiBrowserProxyImpl.getInstance();

    /**
     * Public/mutable for testing.
     * @type {number}
     */
    this.resendDelayMs = 10000;

    /**
     * Public/mutable for testing.
     * @type {number}
     */
    this.maxResendAttempts = 4;

    /**
     * Public for testing.
     * @type {boolean}
     */
    this.feedbackSent = false;

    chrome.feedbackPrivate.getUserEmail(email => {
      this.userEmail_ = email;
    });

    this.browserProxy_.recordEvent(FeedbackEvent.OPENED);
  }

  static get is() {
    return 'feedback-ui';
  }

  static get properties() {
    return {
      /** @private */
      attachLogs_: {
        type: Boolean,
        value: true,
      },

      /** @private */
      audioQuality_: String,

      /** @private */
      comments_: String,

      /**
       * Possible values of |feedbackType_| for use in HTML.
       * @private @const {!Object<string, string>}
       */
      FeedbackType_: {
        type: Object,
        value: FeedbackType,
      },

      /**
       * Controls which set of UI elements is displayed to the user.
       * @private {FeedbackType}
       */
      feedbackType_: {
        type: String,
        value: FeedbackType.BUG,
      },

      /** @private */
      hasNetworkSoftware_: String,

      /** @private */
      networkDescription_: String,

      /** @private */
      logData_: {
        type: String,
        value() {
          return loadTimeData.getString('logData');
        }
      },

      /** @private */
      categoryTag_: {
        type: String,
        value() {
          return loadTimeData.getString('categoryTag');
        }
      },

      /** @private */
      projectedContentUrl_: String,

      /** @private */
      sendDialogText_: String,

      /** @private */
      sendDialogIsInteractive_: Boolean,

      /**
       * Set by onFeedbackChanged_() to control whether the "submit" button is
       * active.
       * @private
       */
      sufficientFeedback_: {
        type: Boolean,
        computed:
            'computeSufficientFeedback_(feedbackType_, videoSmoothness_, ' +
            'videoQuality_, audioQuality_, comments_, visibleInSetup_)',
      },

      /** @private */
      userEmail_: String,

      /** @private */
      videoQuality_: String,

      /** @private */
      videoSmoothness_: String,

      /** @private */
      visibleInSetup_: String,
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shadowRoot.querySelector('.send-logs a')
        .addEventListener('click', event => {
          event.preventDefault();
          this.logsDialog_.showModal();
        });
  }

  /** @private */
  computeSufficientFeedback_() {
    switch (this.feedbackType_) {
      case FeedbackType.MIRRORING_QUALITY:
        return Boolean(
            this.videoSmoothness_ || this.videoQuality_ || this.audioQuality_ ||
            this.comments_);
      case FeedbackType.DISCOVERY:
        return Boolean(this.visibleInSetup_ || this.comments_);
      default:
        return Boolean(this.comments_);
    }
  }

  /**
   * @private
   * @return {boolean}
   */
  showDefaultSection_() {
    switch (this.feedbackType_) {
      case FeedbackType.MIRRORING_QUALITY:
      case FeedbackType.DISCOVERY:
        return false;
      default:
        return true;
    }
  }

  /**
   * @private
   * @return {boolean}
   */
  showMirroringQualitySection_() {
    return this.feedbackType_ === FeedbackType.MIRRORING_QUALITY;
  }

  /**
   * @private
   * @return {boolean}
   */
  showDiscoverySection_() {
    return this.feedbackType_ === FeedbackType.DISCOVERY;
  }

  onSubmit_() {
    const parts = [`Type: ${this.feedbackType_}`, ''];
    const append = (label, value) => {
      if (value) {
        parts.push(`${label}: ${value}`);
      }
    };

    switch (this.feedbackType_) {
      case FeedbackType.MIRRORING_QUALITY:
        append('Video Smoothness', this.videoSmoothness_);
        append('Video Quality', this.videoQuality_);
        append('Audio', this.audioQuality_);
        append('Projected Content/URL', this.projectedContentUrl_);
        append('Comments', this.comments_);
        break;
      case FeedbackType.DISCOVERY:
        append('Chromecast Visible in Setup', this.visibleInSetup_);
        append(
            'Using VPN/proxy/firewall/NAS Software', this.hasNetworkSoftware_);
        append('Network Description', this.networkDescription_);
        append('Comments', this.comments_);
        break;
      default:
        parts.push(this.comments_);
        break;
    }

    const feedback = {
      productId: 85561,
      description: parts.join('\n'),
      email: this.userEmail_,
      flow: chrome.feedbackPrivate.FeedbackFlow.REGULAR,
      categoryTag: this.categoryTag_,
      systemInformation: this.getProductSpecificData_(),
    };
    if (this.attachLogs_) {
      feedback.attachedFile = {
        name: 'log.json',
        data: new Blob([this.logData_]),
      };
    }

    this.updateSendDialog_(FeedbackEvent.SENDING, 'sending', false);
    this.$.sendDialog.showModal();
    this.trySendFeedback_(feedback, 0, 0);
  }

  /**
   * Schedules an attempt to send feedback after |delayMs| milliseconds.
   * @param {!chrome.feedbackPrivate.FeedbackInfo} feedback
   * @param {number} failureCount
   * @param {number} delayMs
   * @private
   */
  trySendFeedback_(feedback, failureCount, delayMs) {
    setTimeout(() => {
      const sendStartTime = Date.now();
      this.browserProxy_.sendFeedback(feedback).then(status => {
        if (status === chrome.feedbackPrivate.Status.SUCCESS) {
          this.feedbackSent = true;
          this.updateSendDialog_(FeedbackEvent.SUCCEEDED, 'sendSuccess', true);
        } else if (failureCount < this.maxResendAttempts) {
          this.updateSendDialog_(FeedbackEvent.RESENDING, 'resending', false);
          const sendDuration = Date.now() - sendStartTime;
          this.trySendFeedback_(
              feedback, failureCount + 1,
              Math.max(0, this.resendDelayMs - sendDuration));
        } else {
          this.updateSendDialog_(FeedbackEvent.FAILED, 'sendFail', true);
        }
      });
    }, delayMs);
  }

  /**
   * Updates the status of the "send" dialog and records the event.
   * @param {FeedbackEvent} event
   * @param {string} stringKey
   * @param {boolean} isInteractive
   * @private
   */
  updateSendDialog_(event, stringKey, isInteractive) {
    this.browserProxy_.recordEvent(event);
    this.sendDialogText_ = loadTimeData.getString(stringKey);
    this.sendDialogIsInteractive_ = isInteractive;
  }

  /** @private */
  onSendDialogOk_() {
    if (this.feedbackSent) {
      chrome.send('close');
    } else {
      this.$.sendDialog.close();
    }
  }

  /** @private */
  onCancel_() {
    if (!this.comments_ ||
        confirm(loadTimeData.getString('discardConfirmation'))) {
      chrome.send('close');
    }
  }

  /** @private */
  onLogsDialogOk_() {
    this.logsDialog_.close();
  }

  /** @private */
  getProductSpecificData_() {
    const data = [{
      key: 'global_media_controls_cast_start_stop',
      value: loadTimeData.getBoolean('globalMediaControlsCastStartStop') ?
          'true' :
          'false',
    }];
    return data;
  }

  /**
   * @private
   * @return {!CrDialogElement}
   */
  get logsDialog_() {
    return /** @type {!CrDialogElement} */ (this.$.logsDialog);
  }

  static get template() {
    // This gets expanded to include the contents of feedback_ui.html by the
    // html_to_js build rule.  It's at the end of the class so line numbers
    // reported in the debugger match the action line numbers in this file.
    return html`{__html_template__}`;
  }
}

customElements.define(FeedbackUiElement.is, FeedbackUiElement);
