// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-bio-enroll-dialog' is a dialog for
 * listing, adding, renaming, and deleting biometric enrollments stored on a
 * security key.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_fingerprint/cr_fingerprint_progress_arc.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared_css.js';
import '../site_favicon.js';
import './security_keys_pin_field.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {Ctap2Status, Enrollment, EnrollmentResponse, SampleResponse, SampleStatus, SecurityKeysBioEnrollProxy, SecurityKeysBioEnrollProxyImpl,} from './security_keys_browser_proxy.js';

/** @enum {string} */
export const BioEnrollDialogPage = {
  INITIAL: 'initial',
  PIN_PROMPT: 'pinPrompt',
  ENROLLMENTS: 'enrollments',
  ENROLL: 'enroll',
  CHOOSE_NAME: 'chooseName',
  ERROR: 'error',
};

Polymer({
  is: 'settings-security-keys-bio-enroll-dialog',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private */
    cancelButtonDisabled_: Boolean,

    /** @private */
    cancelButtonVisible_: Boolean,

    /** @private */
    confirmButtonDisabled_: Boolean,

    /** @private */
    confirmButtonVisible_: Boolean,

    /** @private */
    confirmButtonLabel_: String,

    /** @private */
    deleteInProgress_: Boolean,

    /**
     * The ID of the element currently shown in the dialog.
     * @private {!BioEnrollDialogPage}
     */
    dialogPage_: {
      type: String,
      value: BioEnrollDialogPage.INITIAL,
      observer: 'dialogPageChanged_',
    },

    /** @private */
    doneButtonVisible_: Boolean,

    /**
     * The list of enrollments displayed.
     * @private {!Array<!Enrollment>}
     */
    enrollments_: Array,

    /** @private */
    minPinLength_: Number,

    /** @private */
    progressArcLabel_: String,

    /** @private */
    recentEnrollmentName_: String,
  },

  /** @private {?SecurityKeysBioEnrollProxy} */
  browserProxy_: null,

  /** @private {number} */
  maxSamples_: -1,

  /** @private {string} */
  recentEnrollmentId_: '',

  /** @private {boolean} */
  showSetPINButton_: false,

  /** @override */
  attached() {
    afterNextRender(this, function() {
      IronA11yAnnouncer.requestAvailability();
    });

    this.$.dialog.showModal();
    this.addWebUIListener(
        'security-keys-bio-enroll-error', this.onError_.bind(this));
    this.addWebUIListener(
        'security-keys-bio-enroll-status', this.onEnrollmentSample_.bind(this));
    this.browserProxy_ = SecurityKeysBioEnrollProxyImpl.getInstance();
    this.browserProxy_.startBioEnroll().then(([minPinLength]) => {
      this.minPinLength_ = minPinLength;
      this.dialogPage_ = BioEnrollDialogPage.PIN_PROMPT;
    });
  },

  /**
   * @private
   * @param {string} error
   * @param {boolean=} requiresPINChange
   */
  onError_(error, requiresPINChange = false) {
    this.errorMsg_ = error;
    this.showSetPINButton_ = requiresPINChange;
    this.dialogPage_ = BioEnrollDialogPage.ERROR;
  },

  /** @private */
  submitPIN_() {
    // Disable the confirm button to prevent concurrent submissions.
    this.confirmButtonDisabled_ = true;

    /** @type {!SettingsSecurityKeysPinFieldElement} */ (this.$.pin)
        .trySubmit(pin => this.browserProxy_.providePIN(pin))
        .then(
            () => {
              // Leave confirm button disabled while enumerating fingerprints.
              // It will be re-enabled by dialogPageChanged_() where
              // appropriate.
              this.showEnrollmentsPage_();
            },
            () => {
              // Wrong PIN.
              this.confirmButtonDisabled_ = false;
            });
  },

  /**
   * @private
   * @param {!Array<!Enrollment>} enrollments
   */
  onEnrollments_(enrollments) {
    this.enrollments_ =
        enrollments.slice().sort((a, b) => a.name.localeCompare(b.name));
    this.$.enrollmentList.fire('iron-resize');
    this.dialogPage_ = BioEnrollDialogPage.ENROLLMENTS;
  },

  /** @private */
  dialogPageChanged_() {
    switch (this.dialogPage_) {
      case BioEnrollDialogPage.INITIAL:
        this.cancelButtonVisible_ = true;
        this.cancelButtonDisabled_ = false;
        this.confirmButtonVisible_ = false;
        this.doneButtonVisible_ = false;
        break;
      case BioEnrollDialogPage.PIN_PROMPT:
        this.cancelButtonVisible_ = true;
        this.cancelButtonDisabled_ = false;
        this.confirmButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('continue');
        this.confirmButtonDisabled_ = false;
        this.doneButtonVisible_ = false;
        this.$.pin.focus();
        break;
      case BioEnrollDialogPage.ENROLLMENTS:
        this.cancelButtonVisible_ = false;
        this.confirmButtonVisible_ = false;
        this.doneButtonVisible_ = true;
        break;
      case BioEnrollDialogPage.ENROLL:
        this.cancelButtonVisible_ = true;
        this.cancelButtonDisabled_ = false;
        this.confirmButtonVisible_ = false;
        this.doneButtonVisible_ = false;
        break;
      case BioEnrollDialogPage.CHOOSE_NAME:
        this.cancelButtonVisible_ = false;
        this.confirmButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('continue');
        this.confirmButtonDisabled_ = !this.recentEnrollmentName_.length;
        this.doneButtonVisible_ = false;
        this.$.enrollmentName.focus();
        break;
      case BioEnrollDialogPage.ERROR:
        this.cancelButtonVisible_ = true;
        this.confirmButtonVisible_ = this.showSetPINButton_;
        this.confirmButtonLabel_ = this.i18n('securityKeysSetPinButton');
        this.doneButtonVisible_ = false;
        break;
      default:
        assertNotReached();
    }
    this.fire('bio-enroll-dialog-ready-for-testing');
  },

  /** @private */
  addButtonClick_() {
    assert(this.dialogPage_ === BioEnrollDialogPage.ENROLLMENTS);

    this.maxSamples_ = -1;  // Reset maxSamples_ before enrolling starts.
    /** @type {!CrFingerprintProgressArcElement} */ (this.$.arc).reset();
    this.progressArcLabel_ =
        this.i18n('securityKeysBioEnrollmentEnrollingLabel');

    this.recentEnrollmentId_ = '';
    this.recentEnrollmentName_ = '';

    this.dialogPage_ = BioEnrollDialogPage.ENROLL;

    this.browserProxy_.startEnrolling().then(response => {
      this.onEnrollmentComplete_(response);
    });
  },

  /**
   * @private
   * @param {!SampleResponse} response
   */
  onEnrollmentSample_(response) {
    if (response.status !== SampleStatus.OK) {
      this.progressArcLabel_ =
          this.i18n('securityKeysBioEnrollmentTryAgainLabel');
      this.fire('iron-announce', {text: this.progressArcLabel_});
      return;
    }

    this.progressArcLabel_ =
        this.i18n('securityKeysBioEnrollmentEnrollingLabel');

    assert(response.remaining >= 0);

    if (this.maxSamples_ === -1) {
      this.maxSamples_ = response.remaining + 1;
    }

    /** @type {!CrFingerprintProgressArcElement} */ (this.$.arc)
        .setProgress(
            100 * (this.maxSamples_ - response.remaining - 1) /
                this.maxSamples_,
            100 * (this.maxSamples_ - response.remaining) / this.maxSamples_,
            false);
  },

  /**
   * @private
   * @param {!EnrollmentResponse} response
   */
  onEnrollmentComplete_(response) {
    if (response.code === Ctap2Status.ERR_KEEPALIVE_CANCEL) {
      this.showEnrollmentsPage_();
      return;
    }
    if (response.code !== Ctap2Status.OK) {
      this.onError_(this.i18n('securityKeysBioEnrollmentEnrollingFailedLabel'));
      return;
    }

    this.maxSamples_ = Math.max(this.maxSamples_, 1);
    /** @type {!CrFingerprintProgressArcElement} */ (this.$.arc)
        .setProgress(
            100 * (this.maxSamples_ - 1) / this.maxSamples_, 100, true);

    assert(response.enrollment);
    this.recentEnrollmentId_ = response.enrollment.id;
    this.recentEnrollmentName_ = response.enrollment.name;
    this.cancelButtonVisible_ = false;
    this.confirmButtonVisible_ = true;
    this.confirmButtonDisabled_ = false;
    this.progressArcLabel_ =
        this.i18n('securityKeysBioEnrollmentEnrollingCompleteLabel');
    this.$.confirmButton.focus();
    // Make screen-readers announce enrollment completion.
    this.fire('iron-announce', {text: this.progressArcLabel_});

    this.fire('bio-enroll-dialog-ready-for-testing');
  },

  /** @private */
  confirmButtonClick_() {
    switch (this.dialogPage_) {
      case BioEnrollDialogPage.PIN_PROMPT:
        this.submitPIN_();
        break;
      case BioEnrollDialogPage.ENROLL:
        assert(!!this.recentEnrollmentId_.length);
        this.dialogPage_ = BioEnrollDialogPage.CHOOSE_NAME;
        break;
      case BioEnrollDialogPage.CHOOSE_NAME:
        this.renameNewEnrollment_();
        break;
      case BioEnrollDialogPage.ERROR:
        this.$.dialog.close();
        this.fire('bio-enroll-set-pin');
        break;
      default:
        assertNotReached();
    }
  },

  /** @private */
  renameNewEnrollment_() {
    assert(this.dialogPage_ === BioEnrollDialogPage.CHOOSE_NAME);
    // Disable the confirm button to prevent concurrent submissions. It will
    // be re-enabled by dialogPageChanged_() where appropriate.
    this.confirmButtonDisabled_ = true;
    this.browserProxy_
        .renameEnrollment(this.recentEnrollmentId_, this.recentEnrollmentName_)
        .then(enrollments => {
          this.onEnrollments_(enrollments);
        });
  },

  /** @private */
  showEnrollmentsPage_() {
    this.browserProxy_.enumerateEnrollments().then(enrollments => {
      this.onEnrollments_(enrollments);
    });
  },

  /** @private */
  cancel_() {
    if (this.dialogPage_ === BioEnrollDialogPage.ENROLL) {
      // Cancel an ongoing enrollment.  Will cause the pending
      // enumerateEnrollments() promise to be resolved and proceed to the
      // enrollments page.
      this.cancelButtonDisabled_ = true;
      this.browserProxy_.cancelEnrollment();
    } else {
      // On any other screen, simply close the dialog.
      this.done_();
    }
  },

  /** @private */
  done_() {
    this.$.dialog.close();
  },

  /** @private */
  onDialogClosed_() {
    this.browserProxy_.close();
  },

  /**
   * @private
   * @param {!Event} e
   */
  onIronSelect_(e) {
    // Prevent this event from bubbling since it is unnecessarily triggering
    // the listener within settings-animated-pages.
    e.stopPropagation();
  },

  /**
   * @private
   * @param {!DomRepeatEvent} event
   */
  deleteEnrollment_(event) {
    if (this.deleteInProgress_) {
      return;
    }
    this.deleteInProgress_ = true;
    const enrollment = this.enrollments_[event.model.index];
    this.browserProxy_.deleteEnrollment(enrollment.id).then(enrollments => {
      this.deleteInProgress_ = false;
      this.onEnrollments_(enrollments);
    });
  },

  /** @private */
  onEnrollmentNameInput_() {
    this.confirmButtonDisabled_ = !this.recentEnrollmentName_.length;
  },

  /**
   * @private
   * @param {!BioEnrollDialogPage} dialogPage
   * @return {string} The title string for the current dialog page.
   */
  dialogTitle_(dialogPage) {
    if (dialogPage === BioEnrollDialogPage.ENROLL ||
        dialogPage === BioEnrollDialogPage.CHOOSE_NAME) {
      return this.i18n('securityKeysBioEnrollmentAddTitle');
    }
    return this.i18n('securityKeysBioEnrollmentDialogTitle');
  },

  /**
   * @private
   * @param {?Array} enrollments
   * @return {string} The header label for the enrollments page.
   */
  enrollmentsHeader_(enrollments) {
    return this.i18n(
        enrollments && enrollments.length ?
            'securityKeysBioEnrollmentEnrollmentsLabel' :
            'securityKeysBioEnrollmentNoEnrollmentsLabel');
  },
});
