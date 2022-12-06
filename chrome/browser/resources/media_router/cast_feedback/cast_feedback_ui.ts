// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';

import {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cast_feedback_ui.html.js';

enum FeedbackType {
  BUG = 'Bug',
  FEATURE_REQUEST = 'FeatureRequest',
  MIRRORING_QUALITY = 'MirroringQuality',
  DISCOVERY = 'Discovery',
  OTHER = 'Other',
}

/**
 * Keep in sync with MediaRouterCastFeedbackEvent in enums.xml.
 */
export enum FeedbackEvent {
  OPENED = 0,
  SENDING = 1,
  RESENDING = 2,
  SUCCEEDED = 3,
  FAILED = 4,
  MAX_VALUE = 4,
}

/**
 * See
 * https://docs.google.com/document/d/1c20VYdwpUPyBRQeAS0CMr6ahwWnb0s26gByomOwqDjk
 */
export interface FeedbackUiBrowserProxy {
  /**
   * Records an event using Chrome Metrics.
   */
  recordEvent(event: FeedbackEvent): void;

  /**
   * Proxy for chrome.feedbackPrivate.sendFeedback().
   */
  sendFeedback(info: chrome.feedbackPrivate.FeedbackInfo):
      Promise<chrome.feedbackPrivate.SendFeedbackResult>;
}

export class FeedbackUiBrowserProxyImpl implements FeedbackUiBrowserProxy {
  recordEvent(event: FeedbackEvent) {
    chrome.send(
        'metricsHandler:recordInHistogram',
        ['MediaRouter.Cast.Feedback.Event', event, FeedbackEvent.MAX_VALUE]);
  }

  sendFeedback(info: chrome.feedbackPrivate.FeedbackInfo) {
    return chrome.feedbackPrivate.sendFeedback(
        info, /*loadSystemInfo=*/ undefined, /*formOpenTime=*/ undefined);
  }

  static getInstance(): FeedbackUiBrowserProxy {
    return instance || (instance = new FeedbackUiBrowserProxyImpl());
  }

  static setInstance(obj: FeedbackUiBrowserProxy) {
    instance = obj;
  }
}

let instance: FeedbackUiBrowserProxy|null = null;

// Define static map of local DOM elements that have IDs.
// https://polymer-library.polymer-project.org/3.0/docs/devguide/dom-template#node-finding
export interface FeedbackUiElement {
  $: {
    logsDialog: CrDialogElement,
    sendDialog: CrDialogElement,
  };
}

export class FeedbackUiElement extends PolymerElement {
  static get is() {
    return 'feedback-ui';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      allowContactByEmail_: {
        type: Boolean,
        value: false,
      },

      attachLogs_: {
        type: Boolean,
        value: true,
      },

      audioQuality_: String,
      comments_: String,

      /**
       * Possible values of |FeedbackType| for use in HTML.
       */
      feedbackTypeEnum_: {
        type: Object,
        value: FeedbackType,
      },

      /**
       * Controls which set of UI elements is displayed to the user.
       */
      feedbackType_: {
        type: String,
        value: FeedbackType.BUG,
      },

      hasNetworkSoftware_: String,
      networkDescription_: String,

      logData_: {
        type: String,
        value() {
          return loadTimeData.getString('logData');
        },
      },

      categoryTag_: {
        type: String,
        value() {
          return loadTimeData.getString('categoryTag');
        },
      },

      projectedContentUrl_: String,
      sendDialogText_: String,
      sendDialogIsInteractive_: Boolean,

      /**
       * Set by onFeedbackChanged_() to control whether the "submit" button is
       * active.
       */
      sufficientFeedback_: {
        type: Boolean,
        computed:
            'computeSufficientFeedback_(feedbackType_, videoSmoothness_, ' +
            'videoQuality_, audioQuality_, comments_, visibleInSetup_)',
      },

      userEmail_: String,
      videoQuality_: String,
      videoSmoothness_: String,
      visibleInSetup_: String,
    };
  }

  private allowContactByEmail_: boolean;
  private attachLogs_: boolean;
  private audioQuality_: string;
  private comments_: string;
  private feedbackType_: FeedbackType;
  private hasNetworkSoftware_: string;
  private networkDescription_: string;
  private logData_: string;
  private categoryTag_: string;
  private projectedContentUrl_: string;
  private sendDialogText_: string;
  private sendDialogIsInteractive_: boolean;
  private sufficientFeedback_: boolean;
  private userEmail_: string;
  private videoQuality_: string;
  private videoSmoothness_: string;
  private visibleInSetup_: string;

  private browserProxy_: FeedbackUiBrowserProxy =
      FeedbackUiBrowserProxyImpl.getInstance();

  // Public/mutable for testing.
  resendDelayMs: number = 10000;
  maxResendAttempts: number = 4;
  feedbackSent: boolean = false;

  constructor() {
    super();

    chrome.feedbackPrivate.getUserEmail(email => {
      this.userEmail_ = email;
    });

    this.browserProxy_.recordEvent(FeedbackEvent.OPENED);
  }


  override ready() {
    super.ready();
    this.shadowRoot!.querySelector('#send-logs a')!.addEventListener(
        'click', event => {
          event.preventDefault();
          this.$.logsDialog.showModal();
        });
  }

  private computeSufficientFeedback_() {
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

  private showDefaultSection_(): boolean {
    switch (this.feedbackType_) {
      case FeedbackType.MIRRORING_QUALITY:
      case FeedbackType.DISCOVERY:
        return false;
      default:
        return true;
    }
  }

  private showMirroringQualitySection_(): boolean {
    return this.feedbackType_ === FeedbackType.MIRRORING_QUALITY;
  }

  private showDiscoverySection_(): boolean {
    return this.feedbackType_ === FeedbackType.DISCOVERY;
  }

  private onSubmit_() {
    const parts = [`Type: ${this.feedbackType_}`, ''];

    function append(label: string, value: string) {
      if (value) {
        parts.push(`${label}: ${value}`);
      }
    }

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

    const feedback: chrome.feedbackPrivate.FeedbackInfo = {
      productId: 85561,
      description: parts.join('\n'),
      email: this.allowContactByEmail_ ? this.userEmail_ : '',
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
   */
  private trySendFeedback_(
      feedback: chrome.feedbackPrivate.FeedbackInfo, failureCount: number,
      delayMs: number) {
    setTimeout(() => {
      const sendStartTime = Date.now();
      this.browserProxy_.sendFeedback(feedback).then(result => {
        if (result.status === chrome.feedbackPrivate.Status.SUCCESS) {
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
   */
  private updateSendDialog_(
      event: FeedbackEvent, stringKey: string, isInteractive: boolean) {
    this.browserProxy_.recordEvent(event);
    this.sendDialogText_ = loadTimeData.getString(stringKey);
    this.sendDialogIsInteractive_ = isInteractive;
  }

  private onSendDialogOk_() {
    if (this.feedbackSent) {
      chrome.send('close');
    } else {
      this.$.sendDialog.close();
    }
  }

  private onCancel_() {
    if (!this.comments_ ||
        confirm(loadTimeData.getString('discardConfirmation'))) {
      chrome.send('close');
    }
  }

  private onLogsDialogOk_() {
    this.$.logsDialog.close();
  }

  private getProductSpecificData_(): Array<{key: string, value: string}> {
    const data = [
      {
        key: 'global_media_controls_cast_start_stop',
        value: String(
            !!loadTimeData.getBoolean('globalMediaControlsCastStartStop')),
      },
      {
        key: 'feedbackUserCtlConsent',
        value: String(!!this.allowContactByEmail_),
      },
    ];
    return data;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'feedback-ui': FeedbackUiElement;
  }
}

customElements.define(FeedbackUiElement.is, FeedbackUiElement);
