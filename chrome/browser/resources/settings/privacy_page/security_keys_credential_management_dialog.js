// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-security-keys-credential-management-dialog' is a
 * dialog for viewing and erasing credentials stored on a security key.
 */

cr.define('settings', function() {
  /** @enum {string} */
  const CredentialManagementDialogPage = {
    INITIAL: 'initial',
    PIN_PROMPT: 'pinPrompt',
    CREDENTIALS: 'credentials',
    ERROR: 'error',
  };

  return {
    CredentialManagementDialogPage: CredentialManagementDialogPage,
  };
});

(function() {
'use strict';

const CredentialManagementDialogPage = settings.CredentialManagementDialogPage;

Polymer({
  is: 'settings-security-keys-credential-management-dialog',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * The ID of the element currently shown in the dialog.
     * @private {!settings.CredentialManagementDialogPage}
     */
    dialogPage_: {
      type: String,
      value: CredentialManagementDialogPage.INITIAL,
      observer: 'dialogPageChanged_',
    },

    /**
     * The list of credentials displayed in the dialog.
     * @private {!Array<!Credential>}
     */
    credentials_: Array,

    /**
     * The message displayed on the "error" dialog page.
     * @private
     */
    errorMsg_: String,

    /** @private */
    cancelButtonVisible_: Boolean,

    /** @private */
    confirmButtonVisible_: Boolean,

    /** @private */
    confirmButtonDisabled_: Boolean,

    /** @private */
    confirmButtonLabel_: String,

    /** @private */
    closeButtonVisible_: Boolean,

    /** @private */
    deleteInProgress_: Boolean,
  },

  /** @private {?settings.SecurityKeysCredentialBrowserProxy} */
  browserProxy_: null,

  /** @private {?Set<string>} */
  checkedCredentialIds_: null,

  /** @override */
  attached: function() {
    this.$.dialog.showModal();
    this.addWebUIListener(
        'security-keys-credential-management-finished',
        this.onError_.bind(this));
    this.checkedCredentialIds_ = new Set();
    this.browserProxy_ =
        settings.SecurityKeysCredentialBrowserProxyImpl.getInstance();
    this.browserProxy_.startCredentialManagement().then(
        this.collectPin_.bind(this));
  },

  /** @private */
  collectPin_: function() {
    this.dialogPage_ = CredentialManagementDialogPage.PIN_PROMPT;
    this.$.pin.focus();
  },

  /**
   * @private
   * @param {string} error
   */
  onError_: function(error) {
    this.errorMsg_ = error;
    this.dialogPage_ = CredentialManagementDialogPage.ERROR;
  },

  /** @private */
  submitPIN_: function() {
    if (!this.$.pin.validate()) {
      return;
    }
    this.confirmButtonDisabled_ = true;
    this.browserProxy_.providePIN(this.$.pin.value).then((retries) => {
      this.confirmButtonDisabled_ = false;
      if (retries != null) {
        this.$.pin.showIncorrectPINError(retries);
        this.collectPin_();
        return;
      }
      this.browserProxy_.enumerateCredentials().then(
          this.onCredentials_.bind(this));
    });
  },

  /**
   * @param {number} retries
   * @return {string} localized error string for an invalid PIN attempt and a
   *     given number of remaining retries.
   */
  pinRetriesError_: function(retries) {
    // Warn the user if the number of retries is getting low.
    if (1 < retries && retries <= 3) {
      return this.i18n('securityKeysPINIncorrectRetriesPl', retries.toString());
    }
    return this.i18n(
        retries == 1 ? 'securityKeysPINIncorrectRetriesSin' :
                       'securityKeysPINIncorrect');
  },

  /**
   * @private
   * @param {!Array<!Credential>} credentials
   */
  onCredentials_: function(credentials) {
    if (!credentials.length) {
      this.onError_(this.i18n('securityKeysCredentialManagementNoCredentials'));
      return;
    }
    this.credentials_ = credentials;
    this.$.credentialList.fire('iron-resize');
    this.dialogPage_ = CredentialManagementDialogPage.CREDENTIALS;
  },

  /** @private */
  dialogPageChanged_: function() {
    switch (this.dialogPage_) {
      case CredentialManagementDialogPage.INITIAL:
        this.cancelButtonVisible_ = true;
        this.confirmButtonVisible_ = false;
        this.closeButtonVisible_ = false;
        break;
      case CredentialManagementDialogPage.PIN_PROMPT:
        this.cancelButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('continue');
        this.confirmButtonDisabled_ = false;
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        break;
      case CredentialManagementDialogPage.CREDENTIALS:
        this.cancelButtonVisible_ = true;
        this.confirmButtonLabel_ = this.i18n('delete');
        this.confirmButtonDisabled_ = true;
        this.confirmButtonVisible_ = true;
        this.closeButtonVisible_ = false;
        break;
      case CredentialManagementDialogPage.ERROR:
        this.cancelButtonVisible_ = false;
        this.confirmButtonVisible_ = false;
        this.closeButtonVisible_ = true;
        break;
      default:
        assertNotReached();
    }
    this.fire('credential-management-dialog-ready-for-testing');
  },

  /** @private */
  confirmButtonClick_: function() {
    switch (this.dialogPage_) {
      case CredentialManagementDialogPage.PIN_PROMPT:
        this.submitPIN_();
        break;
      case CredentialManagementDialogPage.CREDENTIALS:
        this.deleteSelectedCredentials_();
        break;
      default:
        assertNotReached();
    }
  },

  /** @private */
  close_: function() {
    this.$.dialog.close();
  },

  /**
   * Stringifies the user entity of a Credential for display in the dialog.
   * @private
   * @param {!Credential} credential
   * @return {string}
   */
  formatUser_: function(credential) {
    if (this.isEmpty_(credential.userDisplayName)) {
      return credential.userName;
    }
    return `${credential.userDisplayName} (${credential.userName})`;
  },

  /** @private */
  onDialogClosed_: function() {
    this.browserProxy_.close();
  },

  /**
   * @private
   * @param {?string} str
   * @return {boolean} Whether this credential has been selected for removal.
   */
  isEmpty_: function(str) {
    return !str || str.length == 0;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onIronSelect_: function(e) {
    // Prevent this event from bubbling since it is unnecessarily triggering the
    // listener within settings-animated-pages.
    e.stopPropagation();
  },

  /**
   * Handler for checking or unchecking a credential.
   * @param {!Event} e
   * @private
   */
  checkedCredentialsChanged_: function(e) {
    const credentialId = e.target.dataset.id;
    if (e.target.checked) {
      this.checkedCredentialIds_.add(credentialId);
    } else {
      this.checkedCredentialIds_.delete(credentialId);
    }
    this.confirmButtonDisabled_ = this.checkedCredentialIds_.size == 0;
  },

  /**
   * @private
   * @param {string} credentialId
   * @return {boolean} true if the checkbox for |credentialId| is checked
   */
  credentialIsChecked_: function(credentialId) {
    return this.checkedCredentialIds_.has(credentialId);
  },

  /** @private */
  deleteSelectedCredentials_: function() {
    assert(this.dialogPage_ == CredentialManagementDialogPage.CREDENTIALS);
    assert(this.credentials_ && this.credentials_.length > 0);
    assert(this.checkedCredentialIds_.size > 0);

    this.confirmButtonDisabled_ = true;
    this.deleteInProgress_ = true;
    this.browserProxy_.deleteCredentials(Array.from(this.checkedCredentialIds_))
        .then((err) => {
          this.confirmButtonDisabled_ = false;
          this.deleteInProgress_ = false;
          this.onError_(err);
        });
  },
});
})();
