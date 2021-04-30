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

export class FeedbackUiElement extends PolymerElement {
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

      /** @private */
      feedbackDescription_: String,

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
      hasNetworkSoftware_: Boolean,

      /** @private */
      logData_: {
        type: String,
        value() {
          return loadTimeData.getString('logData');
        }
      },

      /** @private */
      projectedContentUrl_: String,

      /**
       * Set by onFeedbackChanged_() to control whether the "submit" button is
       * active.
       * @private
       */
      sufficientFeedback_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      userEmail_: String,

      /** @private */
      videoQuality_: String,

      /** @private */
      videoSmoothness_: String,

      /** @private */
      visibleInSetup_: Boolean,
    };
  }

  static get observers() {
    return [
      'onFeedbackChanged_(feedbackType_, videoSmoothness_, ' +
          'videoQuality_, audioQuality_, feedbackDescription_, ' +
          'comments_, visibleInSetup_)',
    ];
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
  onFeedbackChanged_() {
    switch (this.feedbackType_) {
      case FeedbackType.MIRRORING_QUALITY:
        this.sufficientFeedback_ = Boolean(
            this.videoSmoothness_ || this.videoQuality_ || this.audioQuality_ ||
            this.comments_);
        break;
      case FeedbackType.DISCOVERY:
        this.sufficientFeedback_ =
            Boolean(this.visibleInSetup_ || this.comments_);
        break;
      default:
        this.sufficientFeedback_ = Boolean(this.feedbackDescription_);
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

  /** @private */
  onSubmit_() {
    // TODO(jrw): Submit feedback data.
    console.log('onSubmit_');
  }

  /** @private */
  onCancel_() {
    // TODO(jrw): Cancel in-progress submission of feedback data.
    console.log('onCancel_');
  }

  /** @private */
  onLogsDialogOk_() {
    this.logsDialog_.close();
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
