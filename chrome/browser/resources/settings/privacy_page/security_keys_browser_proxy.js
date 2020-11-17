// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * Ctap2Status contains a subset of CTAP2 status codes. See
 * device::CtapDeviceResponseCode for the full list.
 * @enum {number}
 */
export const Ctap2Status = {
  OK: 0x0,
  ERR_INVALID_OPTION: 0x2C,
  ERR_KEEPALIVE_CANCEL: 0x2D,
};

/**
 * Credential represents a CTAP2 resident credential enumerated from a
 * security key.
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
export let Credential;

/**
 * SampleStatus is the result for reading an individual sample ("touch")
 * during a fingerprint enrollment. This is a subset of the
 * lastEnrollSampleStatus enum defined in the CTAP spec.
 * @enum {number}
 */
export const SampleStatus = {
  OK: 0x0,
};

/**
 * SampleResponse indicates the result of an individual sample (sensor touch)
 * for an enrollment suboperation.
 *
 * @typedef {{status: SampleStatus,
 *            remaining: number}}
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
export let SampleResponse;

/**
 * EnrollmentResponse is the final response to an enrollment suboperation,
 *
 * @typedef {{code: Ctap2Status,
 *            enrollment: ?Enrollment}}
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
export let EnrollmentResponse;

/**
 * Enrollment represents a valid fingerprint template stored on a security
 * key, which can be used in a user verification request.
 *
 * @typedef {{name: string,
 *            id: string}}
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
export let Enrollment;

/**
 * SetPINResponse represents the response to startSetPIN and setPIN requests.
 *
 * @typedef {{done: boolean,
 *            error: (number|undefined),
 *            currentMinPinLength: (number|undefined),
 *            newMinPinLength: (number|undefined),
 *            retries: (number|undefined)}}
 * @see chrome/browser/ui/webui/settings/settings_security_key_handler.cc
 */
export let SetPINResponse;

/** @interface */
export class SecurityKeysPINBrowserProxy {
  /**
   * Starts a PIN set/change operation by flashing all security keys. Resolves
   * with a pair of numbers. The first is one if the process has immediately
   * completed (i.e. failed). In this case the second is a CTAP error code.
   * Otherwise the process is ongoing and must be completed by calling
   * |setPIN|. In this case the second number is either the number of tries
   * remaining to correctly specify the current PIN, or else null to indicate
   * that no PIN is currently set.
   * @return {!Promise<!SetPINResponse>}
   */
  startSetPIN() {}

  /**
   * Attempts a PIN set/change operation. Resolves with a pair of numbers
   * whose meaning is the same as with |startSetPIN|. The first number will
   * always be 1 to indicate that the process has completed and thus the
   * second will be the CTAP error code.
   * @return {!Promise<!SetPINResponse>}
   */
  setPIN(oldPIN, newPIN) {}

  /** Cancels all outstanding operations. */
  close() {}
}

/** @interface */
export class SecurityKeysCredentialBrowserProxy {
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
export class SecurityKeysResetBrowserProxy {
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
export class SecurityKeysBioEnrollProxy {
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
   * @return {!Promise<!EnrollmentResponse>} resolves when the
   *     enrollment operation is finished successfully.
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
   * @return {!Promise<!Array<!Enrollment>>} The remaining
   *     enrollments.
   */
  deleteEnrollment(id) {}

  /**
   * Renames the enrollment with the given ID.
   *
   * @param {string} id
   * @param {string} name
   * @return {!Promise<!Array<!Enrollment>>} The updated list of
   *     enrollments.
   */
  renameEnrollment(id, name) {}

  /** Cancels all outstanding operations. */
  close() {}
}

/** @implements {SecurityKeysPINBrowserProxy} */
export class SecurityKeysPINBrowserProxyImpl {
  /** @override */
  startSetPIN() {
    return sendWithPromise('securityKeyStartSetPIN');
  }

  /** @override */
  setPIN(oldPIN, newPIN) {
    return sendWithPromise('securityKeySetPIN', oldPIN, newPIN);
  }

  /** @override */
  close() {
    return chrome.send('securityKeyPINClose');
  }
}

/** @implements {SecurityKeysCredentialBrowserProxy} */
export class SecurityKeysCredentialBrowserProxyImpl {
  /** @override */
  startCredentialManagement() {
    return sendWithPromise('securityKeyCredentialManagementStart');
  }

  /** @override */
  providePIN(pin) {
    return sendWithPromise('securityKeyCredentialManagementPIN', pin);
  }

  /** @override */
  enumerateCredentials() {
    return sendWithPromise('securityKeyCredentialManagementEnumerate');
  }

  /** @override */
  deleteCredentials(ids) {
    return sendWithPromise('securityKeyCredentialManagementDelete', ids);
  }

  /** @override */
  close() {
    return chrome.send('securityKeyCredentialManagementClose');
  }
}

/** @implements {SecurityKeysResetBrowserProxy} */
export class SecurityKeysResetBrowserProxyImpl {
  /** @override */
  reset() {
    return sendWithPromise('securityKeyReset');
  }

  /** @override */
  completeReset() {
    return sendWithPromise('securityKeyCompleteReset');
  }

  /** @override */
  close() {
    return chrome.send('securityKeyResetClose');
  }
}

/** @implements {SecurityKeysBioEnrollProxy} */
export class SecurityKeysBioEnrollProxyImpl {
  /** @override */
  startBioEnroll() {
    return sendWithPromise('securityKeyBioEnrollStart');
  }

  /** @override */
  providePIN(pin) {
    return sendWithPromise('securityKeyBioEnrollProvidePIN', pin);
  }

  /** @override */
  enumerateEnrollments() {
    return sendWithPromise('securityKeyBioEnrollEnumerate');
  }

  /** @override */
  startEnrolling() {
    return sendWithPromise('securityKeyBioEnrollStartEnrolling');
  }

  /** @override */
  cancelEnrollment() {
    return chrome.send('securityKeyBioEnrollCancel');
  }

  /** @override */
  deleteEnrollment(id) {
    return sendWithPromise('securityKeyBioEnrollDelete', id);
  }

  /** @override */
  renameEnrollment(id, name) {
    return sendWithPromise('securityKeyBioEnrollRename', id, name);
  }

  /** @override */
  close() {
    return chrome.send('securityKeyBioEnrollClose');
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
addSingletonGetter(SecurityKeysPINBrowserProxyImpl);
addSingletonGetter(SecurityKeysCredentialBrowserProxyImpl);
addSingletonGetter(SecurityKeysResetBrowserProxyImpl);
addSingletonGetter(SecurityKeysBioEnrollProxyImpl);
