// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-bio-enroll-dialog' is a dialog for
 * listing, adding, renaming, and deleting biometric enrollments stored on a
 * security key.
 */

cr.define('settings', function() {
  /** @enum {string} */
  const BioEnrollDialogPage = {
    INITIAL: 'initial',
    PIN_PROMPT: 'pinPrompt',
    ENROLLMENTS: 'enrollments',
    ENROLL: 'enroll',
    CHOOSE_NAME: 'chooseName',
    ERROR: 'error',
  };

  return {
    BioEnrollDialogPage: BioEnrollDialogPage,
  };
});

(function() {
'use strict';

const BioEnrollDialogPage = settings.BioEnrollDialogPage;

Polymer({
  is: 'settings-security-keys-bio-enroll-dialog',

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
    deleteInProgress_: Boolean,

    /**
     * The ID of the element currently shown in the dialog.
     * @private {!settings.BioEnrollDialogPage}
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
    progressArcLabel_: String,

    /** @private */
    recentEnrollmentName_: String,
  },

  /** @private {?settings.SecurityKeysBioEnrollProxyImpl} */
  browserProxy_: null,

  /** @private {number} */
  maxSamples_: -1,

  /** @private {string} */
  recentEnrollmentId_: '',

  /** @override */
  attached: function() {
    Polymer.RenderStatus.afterNextRender(this, function() {
      Polymer.IronA11yAnnouncer.requestAvailability();
    });

    this.$.dialog.showModal();
    this.addWebUIListener(
        'security-keys-bio-enroll-error', this.onError_.bind(this));
    this.addWebUIListener(
        'security-keys-bio-enroll-status', this.onEnrolling_.bind(this));
    this.browserProxy_ = settings.SecurityKeysBioEnrollProxyImpl.getInstance();
    this.browserProxy_.startBioEnroll().then(() => {
      this.collectPIN_();
    });
  },

  /** @private */
  collectPIN_: function() {
    this.dialogPage_ = BioEnrollDialogPage.PIN_PROMPT;
    this.$.pin.focus();
  },

  /**
   * @private
   * @param {string} error
   */
  onError_: function(error) {
    this.errorMsg_ = error;
    this.dialogPage_ = BioEnrollDialogPage.ERROR;
  },

  /** @private */
  submitPIN_: function() {
    if (!this.$.pin.validate()) {
      this.confirmButtonDisabled_ = false;
      return;
    }
    this.browserProxy_.providePIN(this.$.pin.value).then(retries => {
      this.confirmButtonDisabled_ = false;
      if (retries != null) {
        this.$.pin.showIncorrectPINError(retries);
        return;
      }
      this.showEnrollmentsPage_();
    });
  },

  /**
   * @private
   * @param {!Array<!Enrollment>} enrollments
   */
  onEnrollments_: function(enrollments) {
    this.enrollments_ = enrollments;
    this.$.enrollmentList.fire('iron-resize');
    this.dialogPage_ = BioEnrollDialogPage.ENROLLMENTS;
  },

  /** @private */
  dialogPageChanged_: function() {
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
        this.confirmButtonDisabled_ = false;
        this.doneButtonVisible_ = false;
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
        this.confirmButtonDisabled_ = !this.recentEnrollmentName_.length;
        this.doneButtonVisible_ = false;
        this.$.enrollmentName.focus();
        break;
      case BioEnrollDialogPage.ERROR:
        this.cancelButtonVisible_ = false;
        this.confirmButtonVisible_ = false;
        this.doneButtonVisible_ = true;
        break;
      default:
        assertNotReached();
    }
    this.fire('bio-enroll-dialog-ready-for-testing');
  },

  /** @private */
  addButtonClick_: function() {
    assert(this.dialogPage_ == BioEnrollDialogPage.ENROLLMENTS);

    this.maxSamples_ = -1;  // Reset maxSamples_ before enrolling starts.
    this.$.arc.reset();
    this.progressArcLabel_ =
        this.i18n('securityKeysBioEnrollmentEnrollingLabel');

    this.recentEnrollmentId_ = '';
    this.recentEnrollmentName_ = '';

    this.dialogPage_ = BioEnrollDialogPage.ENROLL;

    this.browserProxy_.startEnrolling().then(response => {
      this.onEnrolling_(response);
    });
  },

  /**
   * @private
   * @param {!EnrollmentStatus} response
   */
  onEnrolling_: function(response) {
    if (response.code == Ctap2Status.ERR_KEEPALIVE_CANCEL) {
      this.showEnrollmentsPage_();
      return;
    }

    if (this.maxSamples_ == -1 && response.status != null) {
      if (response.status == 0) {
        // If the first sample is valid, remaining is one less than max samples
        // required.
        this.maxSamples_ = response.remaining + 1;
      } else {
        // If the first sample failed for any reason (timed out, key full, etc),
        // the remaining number of samples is the max samples required.
        this.maxSamples_ = response.remaining;
      }
    }
    // If 0 samples remain, the enrollment has finished in some state.
    // Currently not checking response['code'] for an error.
    this.$.arc.setProgress(
        100 - (100 * (response.remaining + 1) / this.maxSamples_),
        100 - (100 * response.remaining / this.maxSamples_),
        response.remaining == 0);
    if (response.remaining == 0) {
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
    }
    this.fire('bio-enroll-dialog-ready-for-testing');
  },

  /** @private */
  confirmButtonClick_: function() {
    // Disable |confirmButton| while PIN verification or template enumeration is
    // pending. Resetting |dialogPage_| will re-enable it.
    this.confirmButtonDisabled_ = true;
    switch (this.dialogPage_) {
      case BioEnrollDialogPage.PIN_PROMPT:
        this.submitPIN_();
        break;
      case BioEnrollDialogPage.ENROLL:
        assert(!!this.recentEnrollmentId_.length);
        this.dialogPage_ = BioEnrollDialogPage.CHOOSE_NAME;
        break;
      case BioEnrollDialogPage.CHOOSE_NAME:
        this.browserProxy_
            .renameEnrollment(
                this.recentEnrollmentId_, this.recentEnrollmentName_)
            .then(enrollments => {
              this.onEnrollments_(enrollments);
            });
        break;
      default:
        assertNotReached();
    }
  },

  /** @private */
  showEnrollmentsPage_: function() {
    this.browserProxy_.enumerateEnrollments().then(enrollments => {
      this.onEnrollments_(enrollments);
    });
  },

  /** @private */
  cancel_: function() {
    if (this.dialogPage_ == BioEnrollDialogPage.ENROLL) {
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
  done_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onDialogClosed_: function() {
    this.browserProxy_.close();
  },

  /**
   * @private
   * @param {!Event} e
   */
  onIronSelect_: function(e) {
    // Prevent this event from bubbling since it is unnecessarily triggering the
    // listener within settings-animated-pages.
    e.stopPropagation();
  },

  /**
   * @private
   * @param {!DomRepeatEvent} event
   */
  deleteEnrollment_: function(event) {
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
  onEnrollmentNameInput_: function() {
    this.confirmButtonDisabled_ = !this.recentEnrollmentName_.length;
  },

  /**
   * @private
   * @param {!settings.BioEnrollDialogPage} dialogPage
   * @return {string} The title string for the current dialog page.
   */
  dialogTitle_: function(dialogPage) {
    if (dialogPage == BioEnrollDialogPage.ENROLL ||
        dialogPage == BioEnrollDialogPage.CHOOSE_NAME) {
      return this.i18n('securityKeysBioEnrollmentAddTitle');
    }
    return this.i18n('securityKeysBioEnrollmentDialogTitle');
  },

  /**
   * @private
   * @param {?Array} enrollments
   * @return {string} The header label for the enrollments page.
   */
  enrollmentsHeader_: function(enrollments) {
    return this.i18n(
        enrollments && enrollments.length ?
            'securityKeysBioEnrollmentEnrollmentsLabel' :
            'securityKeysBioEnrollmentNoEnrollmentsLabel');
  },
});
})();
