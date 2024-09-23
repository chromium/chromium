// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-bio-enroll-dialog' is a dialog for
 * listing, adding, renaming, and deleting biometric enrollments stored on a
 * security key.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared.css.js';
import '../site_favicon.js';
import '../i18n_setup.js';
import './fingerprint_progress_arc.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {FingerprintProgressArcElement} from './fingerprint_progress_arc.js';
import {getTemplate} from './security_keys_bio_enroll_dialog.html.js';
import type {Enrollment, EnrollmentResponse, SampleResponse, SecurityKeysBioEnrollProxy} from './security_keys_browser_proxy.js';
import {Ctap2Status, SampleStatus, SecurityKeysBioEnrollProxyImpl} from './security_keys_browser_proxy.js';
import type {SettingsSecurityKeysPinFieldElement} from './security_keys_pin_field.js';

export enum BioEnrollDialogPage {
  INITIAL = 'initial',
  PIN_PROMPT = 'pinPrompt',
  ENROLLMENTS = 'enrollments',
  ENROLL = 'enroll',
  CHOOSE_NAME = 'chooseName',
  ERROR = 'error',
}

export interface SettingsSecurityKeysBioEnrollDialogElement {
  $: {
    addButton: HTMLElement,
    arc: FingerprintProgressArcElement,
    cancelButton: CrButtonElement,
    confirmButton: CrButtonElement,
    dialog: CrDialogElement,
    error: HTMLElement,
    enrollmentList: IronListElement,
    enrollmentName: CrInputElement,
    pin: SettingsSecurityKeysPinFieldElement,
  };
}

const SettingsSecurityKeysBioEnrollDialogElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class SettingsSecurityKeysBioEnrollDialogElement extends
    SettingsSecurityKeysBioEnrollDialogElementBase {
  static get is() {
    return 'settings-security-keys-bio-enroll-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      cancelButtonDisabled_: Boolean,
      cancelButtonVisible_: Boolean,
      confirmButtonDisabled_: Boolean,
      confirmButtonVisible_: Boolean,
      confirmButtonLabel_: String,
      deleteInProgress_: Boolean,

      /**
       * The ID of the element currently shown in the dialog.
       */
      dialogPage_: {
        type: String,
        value: BioEnrollDialogPage.INITIAL,
        observer: 'dialogPageChanged_',
      },

      doneButtonVisible_: Boolean,

      /**
       * The list of enrollments displayed.
       */
      enrollments_: Array,

      minPinLength_: Number,
      progressArcLabel_: String,
      recentEnrollmentName_: String,
      enrollmentNameError_: String,
      enrollmentNameMaxUtf8Length_: Number,
      errorMsg_: String,
    };
  }

  private cancelButtonDisabled_: boolean;
  private cancelButtonVisible_: boolean;
  private confirmButtonDisabled_: boolean;
  private confirmButtonVisible_: boolean;
  private confirmButtonLabel_: string;
  private deleteInProgress_: boolean;
  private dialogPage_: BioEnrollDialogPage;
  private doneButtonVisible_: boolean;
  private enrollments_: Enrollment[];
  private minPinLength_: number;
  private progressArcLabel_: string;
  private recentEnrollmentName_: string;
  private enrollmentNameError_: string|null;
  private enrollmentNameMaxUtf8Length_: number;
  private errorMsg_: string;

  private browserProxy_: SecurityKeysBioEnrollProxy =
      SecurityKeysBioEnrollProxyImpl.getInstance();
  private maxSamples_: number = -1;
  private recentEnrollmentId_: string = '';
  private showSetPINButton_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
    this.addWebUiListener(
        'security-keys-bio-enroll-error',
        (error: string, requiresPINChange = false) =>
            this.onError_(error, requiresPINChange));
    this.addWebUiListener(
        'security-keys-bio-enroll-status',
        (response: SampleResponse) => this.onEnrollmentSample_(response));
    this.browserProxy_.startBioEnroll().then(([minPinLength]) => {
      this.minPinLength_ = minPinLength;
      this.dialogPage_ = BioEnrollDialogPage.PIN_PROMPT;
    });
  }

  setDialogPageForTesting(page: BioEnrollDialogPage) {
    this.dialogPage_ = page;
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private onError_(error: string, requiresPINChange = false) {
    this.errorMsg_ = error;
    this.showSetPINButton_ = requiresPINChange;
    this.dialogPage_ = BioEnrollDialogPage.ERROR;
  }

  private submitPin_() {
    // Disable the confirm button to prevent concurrent submissions.
    this.confirmButtonDisabled_ = true;

    this.$.pin.trySubmit(pin => this.browserProxy_.providePin(pin))
        .then(
            () => {
              this.browserProxy_.getSensorInfo().then(sensorInfo => {
                this.enrollmentNameMaxUtf8Length_ =
                    sensorInfo.maxTemplateFriendlyName;
                // Leave confirm button disabled while enumerating fingerprints.
                // It will be re-enabled by dialogPageChanged_() where
                // appropriate.
                this.showEnrollmentsPage_();
              });
            },
            () => {
              // Wrong PIN.
              this.confirmButtonDisabled_ = false;
            });
  }

  private onEnrollments_(enrollments: Enrollment[]) {
    this.enrollments_ =
        enrollments.slice().sort((a, b) => a.name.localeCompare(b.name));
    this.$.enrollmentList.fire('iron-resize');
    this.dialogPage_ = BioEnrollDialogPage.ENROLLMENTS;
  }

  setCancelButtonDisabledForTesting(disabled: boolean) {
    this.cancelButtonDisabled_ = disabled;
  }

  private dialogPageChanged_() {
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
    this.fire_('bio-enroll-dialog-ready-for-testing');
  }

  private addButtonClick_() {
    assert(this.dialogPage_ === BioEnrollDialogPage.ENROLLMENTS);

    this.maxSamples_ = -1;  // Reset maxSamples_ before enrolling starts.
    this.$.arc.reset();
    this.progressArcLabel_ =
        this.i18n('securityKeysBioEnrollmentEnrollingLabel');

    this.recentEnrollmentId_ = '';
    this.recentEnrollmentName_ = '';

    this.dialogPage_ = BioEnrollDialogPage.ENROLL;

    this.browserProxy_.startEnrolling().then(response => {
      this.onEnrollmentComplete_(response);
    });
  }

  private onEnrollmentSample_(response: SampleResponse) {
    if (response.status !== SampleStatus.OK) {
      this.progressArcLabel_ =
          this.i18n('securityKeysBioEnrollmentTryAgainLabel');
      getAnnouncerInstance().announce(this.progressArcLabel_);
      return;
    }

    this.progressArcLabel_ =
        this.i18n('securityKeysBioEnrollmentEnrollingLabel');

    assert(response.remaining >= 0);

    if (this.maxSamples_ === -1) {
      this.maxSamples_ = response.remaining + 1;
    }

    this.$.arc.setProgress(
        100 * (this.maxSamples_ - response.remaining - 1) / this.maxSamples_,
        100 * (this.maxSamples_ - response.remaining) / this.maxSamples_,
        false);
  }

  private onEnrollmentComplete_(response: EnrollmentResponse) {
    switch (response.code) {
      case Ctap2Status.OK:
        break;
      case Ctap2Status.ERR_KEEPALIVE_CANCEL:
        this.showEnrollmentsPage_();
        return;
      case Ctap2Status.ERR_FP_DATABASE_FULL:
        this.onError_(this.i18n('securityKeysBioEnrollmentStorageFullLabel'));
        return;
      default:
        this.onError_(
            this.i18n('securityKeysBioEnrollmentEnrollingFailedLabel'));
        return;
    }

    this.maxSamples_ = Math.max(this.maxSamples_, 1);
    this.$.arc.setProgress(
        100 * (this.maxSamples_ - 1) / this.maxSamples_, 100, true);

    assert(response.enrollment);
    this.recentEnrollmentId_ = response.enrollment!.id;
    this.recentEnrollmentName_ = response.enrollment!.name;
    this.cancelButtonVisible_ = false;
    this.confirmButtonVisible_ = true;
    this.confirmButtonDisabled_ = false;
    this.progressArcLabel_ =
        this.i18n('securityKeysBioEnrollmentEnrollingCompleteLabel');
    this.$.confirmButton.focus();
    // Make screen-readers announce enrollment completion.
    this.fire_('iron-announce', {text: this.progressArcLabel_});

    this.fire_('bio-enroll-dialog-ready-for-testing');
  }

  private confirmButtonClick_() {
    switch (this.dialogPage_) {
      case BioEnrollDialogPage.PIN_PROMPT:
        this.submitPin_();
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
        this.fire_('bio-enroll-set-pin');
        break;
      default:
        assertNotReached();
    }
  }

  private renameNewEnrollment_() {
    assert(this.dialogPage_ === BioEnrollDialogPage.CHOOSE_NAME);

    // Check that the user-provided name doesn't exceed the maximum permissible
    // length reported by the security key when encoded as UTF-8. (Note that
    // JavaScript String length counts code units, but string length maximums in
    // CTAP 2.1 are generally on UTF-8 bytes.)
    if (new TextEncoder().encode(this.recentEnrollmentName_).length >
        this.enrollmentNameMaxUtf8Length_) {
      this.enrollmentNameError_ =
          this.i18n('securityKeysBioEnrollmentNameLabelTooLong');
      return;
    }
    this.enrollmentNameError_ = null;

    // Disable the confirm button to prevent concurrent submissions. It will
    // be re-enabled by dialogPageChanged_() where appropriate.
    this.confirmButtonDisabled_ = true;
    this.browserProxy_
        .renameEnrollment(this.recentEnrollmentId_, this.recentEnrollmentName_)
        .then(enrollments => {
          this.onEnrollments_(enrollments);
        });
  }

  private showEnrollmentsPage_() {
    this.browserProxy_.enumerateEnrollments().then(enrollments => {
      this.onEnrollments_(enrollments);
    });
  }

  private cancel_() {
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
  }

  private done_() {
    this.$.dialog.close();
  }

  private onDialogClosed_() {
    this.browserProxy_.close();
  }

  private onIronSelect_(e: Event) {
    // Prevent this event from bubbling since it is unnecessarily triggering
    // the listener within settings-animated-pages.
    e.stopPropagation();

    // Also asynchronously notify iron-list of the possible resize.
    setTimeout(() => this.$.enrollmentList.notifyResize(), 0);
  }

  private deleteEnrollment_(event: {model: {index: number}}) {
    if (this.deleteInProgress_) {
      return;
    }
    this.deleteInProgress_ = true;
    const enrollment = this.enrollments_[event.model.index];
    this.browserProxy_.deleteEnrollment(enrollment.id).then(enrollments => {
      this.deleteInProgress_ = false;
      this.onEnrollments_(enrollments);
    });
  }

  private onEnrollmentNameInput_() {
    this.confirmButtonDisabled_ = !this.recentEnrollmentName_.length;
  }

  /**
   * @return The title string for the current dialog page.
   */
  private dialogTitle_(dialogPage: BioEnrollDialogPage): string {
    if (dialogPage === BioEnrollDialogPage.ENROLL ||
        dialogPage === BioEnrollDialogPage.CHOOSE_NAME) {
      return this.i18n('securityKeysBioEnrollmentAddTitle');
    }
    return this.i18n('securityKeysBioEnrollmentDialogTitle');
  }

  /**
   * @return The header label for the enrollments page.
   */
  private enrollmentsHeader_(enrollments: Enrollment[]|null): string {
    return this.i18n(
        enrollments && enrollments.length ?
            'securityKeysBioEnrollmentEnrollmentsLabel' :
            'securityKeysBioEnrollmentNoEnrollmentsLabel');
  }

  private isNullOrEmpty_(s: string): boolean {
    return s === '' || !s;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-keys-bio-enroll-dialog':
        SettingsSecurityKeysBioEnrollDialogElement;
  }
}

customElements.define(
    SettingsSecurityKeysBioEnrollDialogElement.is,
    SettingsSecurityKeysBioEnrollDialogElement);
