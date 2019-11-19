// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * Ctap2Status contains a subset of CTAP2 status codes. See
 * device::CtapDeviceResponseCode for the full list.
 * @enum {number}
 */
const Ctap2Status = {
  OK: 0x0,
  ERR_KEEPALIVE_CANCEL: 0x2D,
};

/**
 * Credential represents a CTAP2 resident credential enumerated from a security
 * key.
 *
 * id: (required) The hex encoding of the CBOR-serialized
 *     PublicKeyCredentialDescriptor of the credential.
 *
 * relyingPartyId: (required) The RP ID (i.e. the site that created the
 *     credential; eTLD+n)
 *
 * userName: (required) The PublicKeyCredentialUserEntity.name
 *
 * userDisplayName: (required) The PublicKeyCredentialUserEntity.display_name
 *
 * @typedef {{id: string,
 *            relyingPartyId: string,
 *            userName: string,
 *            userDisplayName: string}}
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
let Credential;

/**
 * EnrollmentStatus represents the current status of an enrollment suboperation,
 * where 'remaining' is the number of samples left, 'status' is the last
 * enrollment status, 'code' indicates the final CtapDeviceResponseCode of the
 * operation, and 'enrollment' contains the new Enrollment.
 *
 * For each enrollment sample, 'status' is set - when the enrollment operation
 * reaches an end state, 'code' and, if successful, 'enrollment' are set. |OK|
 * indicates successful enrollment. A code of |ERR_KEEPALIVE_CANCEL| indicates
 * user-initated cancellation.
 *
 * @typedef {{status: ?number,
 *            code: ?Ctap2Status,
 *            remaining: number,
 *            enrollment: ?Enrollment}}
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
let EnrollmentStatus;

/**
 * Enrollment represents a valid fingerprint template stored on a security key,
 * which can be used in a user verification request.
 *
 * @typedef {{name: string,
 *            id: string}}
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
let Enrollment;

cr.define('settings', function() {
  /** @interface */
  class SecurityKeysPINBrowserProxy {
    /**
     * Starts a PIN set/change operation by flashing all security keys. Resolves
     * with a pair of numbers. The first is one if the process has immediately
     * completed (i.e. failed). In this case the second is a CTAP error code.
     * Otherwise the process is ongoing and must be completed by calling
     * |setPIN|. In this case the second number is either the number of tries
     * remaining to correctly specify the current PIN, or else null to indicate
     * that no PIN is currently set.
     * @return {!Promise<!Array<number>>}
     */
    startSetPIN() {}

    /**
     * Attempts a PIN set/change operation. Resolves with a pair of numbers
     * whose meaning is the same as with |startSetPIN|. The first number will
     * always be 1 to indicate that the process has completed and thus the
     * second will be the CTAP error code.
     * @return {!Promise<!Array<number>>}
     */
    setPIN(oldPIN, newPIN) {}

    /** Cancels all outstanding operations. */
    close() {}
  }

  /** @interface */
  class SecurityKeysCredentialBrowserProxy {
    /**
     * Starts a credential management operation.
     *
     * Callers must listen to errors that can occur during the operation via a
     * 'security-keys-credential-management-error' WebListener. Values received
     * via this listener are localized error strings. When the
     * WebListener fires, the operation must be considered terminated.
     *
     * @return {!Promise} a promise that resolves when the handler is ready for
     *     the authenticator PIN to be provided.
     */
    startCredentialManagement() {}

    /**
     * Provides a PIN for a credential management operation. The
     * startCredentialManagement() promise must have resolved before this method
     * may be called.
     * @return {!Promise<?number>} a promise that resolves with null if the PIN
     *     was correct or the number of retries remaining otherwise.
     */
    providePIN(pin) {}

    /**
     * Enumerates credentials on the authenticator. A correct PIN must have
     * previously been supplied via providePIN() before this
     * method may be called.
     * @return {!Promise<!Array<!Credential>>}
     */
    enumerateCredentials() {}

    /**
     * Deletes the credentials with the given IDs from the security key.
     * @param {!Array<string>} ids
     * @return {!Promise<string>} a localized response message to display to
     *     the user (on either success or error)
     */
    deleteCredentials(ids) {}

    /** Cancels all outstanding operations. */
    close() {}
  }

  /** @interface */
  class SecurityKeysResetBrowserProxy {
    /**
     * Starts a reset operation by flashing all security keys and sending a
     * reset command to the one that the user activates. Resolves with a CTAP
     * error code.
     * @return {!Promise<number>}
     */
    reset() {}

    /**
     * Waits for a reset operation to complete. Resolves with a CTAP error code.
     * @return {!Promise<number>}
     */
    completeReset() {}

    /** Cancels all outstanding operations. */
    close() {}
  }

  /** @interface */
  class SecurityKeysBioEnrollProxy {
    /**
     * Starts a biometric enrollment operation.
     *
     * Callers must listen to errors that can occur during this operation via a
     * 'security-keys-bio-enrollment-error' WebUIListener. Values received via
     * this listener are localized error strings. The WebListener may fire at
     * any point during the operation (enrolling, deleting, etc) and when it
     * fires, the operation must be considered terminated.
     *
     * @return {!Promise} resolves when the handler is ready for the
     *     authentcation PIN to be provided.
     */
    startBioEnroll() {}

    /**
     * Provides a PIN for a biometric enrollment operation. The startBioEnroll()
     * Promise must have resolved before this method may be called.
     *
     * @return {!Promise<?number>} resolves with null if the PIN was correct,
     *     the number of retries remaining otherwise.
     */
    providePIN(pin) {}

    /**
     * Enumerates enrollments on the authenticator. A correct PIN must have
     * previously been supplied via bioEnrollProvidePIN() before this method may
     * be called.
     *
     * @return {!Promise<!Array<!Enrollment>>}
     */
    enumerateEnrollments() {}

    /**
     * Move the operation into enrolling mode, which instructs the authenticator
     * to start sampling for touches.
     *
     * Callers must listen to status updates that will occur during this
     * suboperation via a 'security-keys-bio-enroll-status' WebListener. Values
     * received via this listener are DictionaryValues with two elements (see
     * below). When the WebListener fires, the authenticator has either timed
     * out waiting for a touch, or has successfully processed a touch. Any
     * errors will fire the 'security-keys-bio-enrollment-error' WebListener.
     *
     * @return {!Promise<!EnrollmentStatus>} resolves when the enrollment
     *     operation is finished successfully.
     */
    startEnrolling() {}

    /**
     * Cancel an ongoing enrollment suboperation. This can safely be called at
     * any time and only has an impact when the authenticator is currently
     * sampling.
     */
    cancelEnrollment() {}

    /**
     * Deletes the enrollment with the given ID.
     *
     * @param {string} id
     * @return {!Promise<!Array<!Enrollment>>} The remaining enrollments.
     */
    deleteEnrollment(id) {}

    /**
     * Renames the enrollment with the given ID.
     *
     * @param {string} id
     * @param {string} name
     * @return {!Promise<!Array<!Enrollment>>} The updated list of enrollments.
     */
    renameEnrollment(id, name) {}

    /** Cancels all outstanding operations. */
    close() {}
  }

  /** @implements {settings.SecurityKeysPINBrowserProxy} */
  class SecurityKeysPINBrowserProxyImpl {
    /** @override */
    startSetPIN() {
      return cr.sendWithPromise('securityKeyStartSetPIN');
    }

    /** @override */
    setPIN(oldPIN, newPIN) {
      return cr.sendWithPromise('securityKeySetPIN', oldPIN, newPIN);
    }

    /** @override */
    close() {
      return chrome.send('securityKeyPINClose');
    }
  }

  /** @implements {settings.SecurityKeysCredentialBrowserProxy} */
  class SecurityKeysCredentialBrowserProxyImpl {
    /** @override */
    startCredentialManagement() {
      return cr.sendWithPromise('securityKeyCredentialManagementStart');
    }

    /** @override */
    providePIN(pin) {
      return cr.sendWithPromise('securityKeyCredentialManagementPIN', pin);
    }

    /** @override */
    enumerateCredentials() {
      return cr.sendWithPromise('securityKeyCredentialManagementEnumerate');
    }

    /** @override */
    deleteCredentials(ids) {
      return cr.sendWithPromise('securityKeyCredentialManagementDelete', ids);
    }

    /** @override */
    close() {
      return chrome.send('securityKeyCredentialManagementClose');
    }
  }

  /** @implements {settings.SecurityKeysResetBrowserProxy} */
  class SecurityKeysResetBrowserProxyImpl {
    /** @override */
    reset() {
      return cr.sendWithPromise('securityKeyReset');
    }

    /** @override */
    completeReset() {
      return cr.sendWithPromise('securityKeyCompleteReset');
    }

    /** @override */
    close() {
      return chrome.send('securityKeyResetClose');
    }
  }

  /** @implements {settings.SecurityKeysBioEnrollProxy} */
  class SecurityKeysBioEnrollProxyImpl {
    /** @override */
    startBioEnroll() {
      return cr.sendWithPromise('securityKeyBioEnrollStart');
    }

    /** @override */
    providePIN(pin) {
      return cr.sendWithPromise('securityKeyBioEnrollProvidePIN', pin);
    }

    /** @override */
    enumerateEnrollments() {
      return cr.sendWithPromise('securityKeyBioEnrollEnumerate');
    }

    /** @override */
    startEnrolling() {
      return cr.sendWithPromise('securityKeyBioEnrollStartEnrolling');
    }

    /** @override */
    cancelEnrollment() {
      return chrome.send('securityKeyBioEnrollCancel');
    }

    /** @override */
    deleteEnrollment(id) {
      return cr.sendWithPromise('securityKeyBioEnrollDelete', id);
    }

    /** @override */
    renameEnrollment(id, name) {
      return cr.sendWithPromise('securityKeyBioEnrollRename', id, name);
    }

    /** @override */
    close() {
      return chrome.send('securityKeyBioEnrollClose');
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  cr.addSingletonGetter(SecurityKeysPINBrowserProxyImpl);
  cr.addSingletonGetter(SecurityKeysCredentialBrowserProxyImpl);
  cr.addSingletonGetter(SecurityKeysResetBrowserProxyImpl);
  cr.addSingletonGetter(SecurityKeysBioEnrollProxyImpl);

  return {
    SecurityKeysPINBrowserProxy: SecurityKeysPINBrowserProxy,
    SecurityKeysPINBrowserProxyImpl: SecurityKeysPINBrowserProxyImpl,
    SecurityKeysCredentialBrowserProxy: SecurityKeysCredentialBrowserProxy,
    SecurityKeysCredentialBrowserProxyImpl:
        SecurityKeysCredentialBrowserProxyImpl,
    SecurityKeysResetBrowserProxy: SecurityKeysResetBrowserProxy,
    SecurityKeysResetBrowserProxyImpl: SecurityKeysResetBrowserProxyImpl,
    SecurityKeysBioEnrollProxy: SecurityKeysBioEnrollProxy,
    SecurityKeysBioEnrollProxyImpl: SecurityKeysBioEnrollProxyImpl,
  };
});
