// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles web page requests for gnubby enrollment.
 */

'use strict';

/**
 * webSafeBase64ToNormal reencodes a base64-encoded string.
 *
 * @param {string} s A string encoded as web-safe base64.
 * @return {string} A string encoded in normal base64.
 */
function webSafeBase64ToNormal(s) {
  return s.replace(/-/g, '+').replace(/_/g, '/');
}

/**
 * decodeWebSafeBase64ToArray decodes a base64-encoded string.
 *
 * @param {string} s A base64-encoded string.
 * @return {!Uint8Array}
 */
function decodeWebSafeBase64ToArray(s) {
  var bytes = atob(webSafeBase64ToNormal(s));
  var buffer = new ArrayBuffer(bytes.length);
  var ret = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; i++) {
    ret[i] = bytes.charCodeAt(i);
  }
  return ret;
}

// See "FIDO U2F Authenticator Transports Extension", §3.2.1.
const transportTypeOID = [1, 3, 6, 1, 4, 1, 45724, 2, 1, 1];

/**
 * Returns the value of the transport-type X.509 extension from the supplied
 * attestation certificate, or 0.
 *
 * @param {!Uint8Array} der The DER bytes of an attestation certificate.
 * @returns {Uint8Array} the bytes of the transport-type extension, if present,
 *     or null.
 * @throws {Error}
 */
function transportType(der) {
  var topLevel = new ByteString(der);
  const tbsCert = topLevel.getASN1(Tag.SEQUENCE).getASN1(Tag.SEQUENCE);
  tbsCert.getOptionalASN1(
      Tag.CONSTRUCTED | Tag.CONTEXT_SPECIFIC | 0);  // version
  tbsCert.getASN1(Tag.INTEGER);                     // serialNumber
  tbsCert.getASN1(Tag.SEQUENCE);                    // signature algorithm
  tbsCert.getASN1(Tag.SEQUENCE);                    // issuer
  tbsCert.getASN1(Tag.SEQUENCE);                    // validity
  tbsCert.getASN1(Tag.SEQUENCE);                    // subject
  tbsCert.getASN1(Tag.SEQUENCE);                    // SPKI
  tbsCert.getOptionalASN1(                          // issuerUniqueID
      Tag.CONSTRUCTED | Tag.CONTEXT_SPECIFIC | 1);
  tbsCert.getOptionalASN1(  // subjectUniqueID
      Tag.CONSTRUCTED | Tag.CONTEXT_SPECIFIC | 2);
  const outerExtensions =
      tbsCert.getOptionalASN1(Tag.CONSTRUCTED | Tag.CONTEXT_SPECIFIC | 3);
  if (outerExtensions == null) {
    return null;
  }
  const extensions = outerExtensions.getASN1(Tag.SEQUENCE);
  if (extensions.empty) {
    return null;
  }

  while (!extensions.empty) {
    const extension = extensions.getASN1(Tag.SEQUENCE);
    const oid = extension.getASN1ObjectIdentifier();
    if (oid.length != transportTypeOID.length) {
      continue;
    }
    var matches = true;
    for (var i = 0; i < oid.length; i++) {
      if (oid[i] != transportTypeOID[i]) {
        matches = false;
        break;
      }
    }
    if (!matches) {
      continue;
    }

    extension.getOptionalASN1(Tag.BOOLEAN);  // 'critical' flag
    const contents = extension.getASN1(Tag.OCTETSTRING);
    if (!extension.empty) {
      throw Error('trailing garbage after extension');
    }
    return contents.getASN1(Tag.BITSTRING).data;
  }
  return null;
}

/**
 * makeCertAndKey creates a new ECDSA keypair and returns the private key
 * and a cert containing the public key.
 *
 * @param {!Uint8Array} original The certificate being replaced, as DER bytes.
 * @return {Promise<{privateKey: !webCrypto.CryptoKey, certDER: !Uint8Array}>}
 */
async function makeCertAndKey(original) {
  var transport = transportType(original);
  if (transport !== null) {
    if (transport.length != 2) {
      throw Error('bad extension length');
    }
    if (transport[0] < 3) {
      throw Error('too many bits set');  // Only 5 bits are defined.
    }
  }

  const keyalg = {name: 'ECDSA', namedCurve: 'P-256'};
  const keypair =
      await crypto.subtle.generateKey(keyalg, true, ['sign', 'verify']);
  const publicKey = await crypto.subtle.exportKey('raw', keypair.publicKey);
  var serialBuffer = new ArrayBuffer(10);
  var serial = new Uint8Array(serialBuffer);
  crypto.getRandomValues(serial);

  const ecdsaWithSHA256 = [1, 2, 840, 10045, 4, 3, 2];
  const ansiX962 = [1, 2, 840, 10045, 2, 1];
  const secp256R1 = [1, 2, 840, 10045, 3, 1, 7];
  const commonName = [2, 5, 4, 3];
  const x509V3 = 2;

  const certBuilder = new ByteBuilder();
  certBuilder.addASN1(Tag.SEQUENCE, (b) => {
    b.addASN1(Tag.SEQUENCE, (b) => {  // TBSCertificate
      b.addASN1(Tag.CONTEXT_SPECIFIC | Tag.CONSTRUCTED | 0, (b) => {
        b.addASN1Int(x509V3);  // Version
      });
      b.addASN1BigInt(serial);          // Serial number
      b.addASN1(Tag.SEQUENCE, (b) => {  // Signature algorithm
        b.addASN1ObjectIdentifier(ecdsaWithSHA256);
      });
      b.addASN1(Tag.SEQUENCE, (b) => {  // Issuer
        b.addASN1(Tag.SET, (b) => {
          b.addASN1(Tag.SEQUENCE, (b) => {
            b.addASN1ObjectIdentifier(commonName);
            b.addASN1PrintableString('U2F Issuer');
          });
        });
      });
      b.addASN1(Tag.SEQUENCE, (b) => {  // Validity
        b.addASN1(Tag.UTCTime, (b) => {
          b.addBytesFromString('0001010000Z');
        });
        b.addASN1(Tag.UTCTime, (b) => {
          b.addBytesFromString('0001010000Z');
        });
      });
      b.addASN1(Tag.SEQUENCE, (b) => {  // Subject
        b.addASN1(Tag.SET, (b) => {
          b.addASN1(Tag.SEQUENCE, (b) => {
            b.addASN1ObjectIdentifier(commonName);
            b.addASN1PrintableString('U2F Device');
          });
        });
      });
      b.addASN1(Tag.SEQUENCE, (b) => {    // Public key
        b.addASN1(Tag.SEQUENCE, (b) => {  // Algorithm identifier
          b.addASN1ObjectIdentifier(ansiX962);
          b.addASN1ObjectIdentifier(secp256R1);
        });
        b.addASN1BitString(new Uint8Array(publicKey));
      });
      if (transport !== null) {
        var t = transport;  // This causes the compiler to see t cannot be null.
        // Extensions
        b.addASN1(Tag.CONTEXT_SPECIFIC | Tag.CONSTRUCTED | 3, (b) => {
          b.addASN1(Tag.SEQUENCE, (b) => {
            b.addASN1(Tag.SEQUENCE, (b) => {  // Transport-type extension.
              b.addASN1ObjectIdentifier(transportTypeOID);
              b.addASN1(Tag.OCTETSTRING, (b) => {
                b.addASN1(Tag.BITSTRING, (b) => {
                  b.addBytes(t);
                });
              });
            });
          });
        });
      }
    });
    b.addASN1(Tag.SEQUENCE, (b) => {  // Algorithm identifier
      b.addASN1ObjectIdentifier(ecdsaWithSHA256);
    });
    b.addASN1(Tag.BITSTRING, (b) => {  // Signature
      // This signature is obviously not correct since it's constant and the
      // rest of the certificate is not. However, since the issuer certificate
      // doesn't exist, there's no way for anyone to check the signature on this
      // certificate and thus this sufficies. However, at least fastmail.com
      // expects to be able to parse out a valid ECDSA signature and so one is
      // provided.
      b.addBytes(new Uint8Array([
        0x00, 0x30, 0x45, 0x02, 0x21, 0x00, 0xc1, 0xa3, 0xa6, 0x8e, 0x2f,
        0x16, 0xa7, 0x21, 0x46, 0x27, 0x05, 0x7f, 0x62, 0xbb, 0x72, 0x8c,
        0x9e, 0x03, 0xe7, 0xa1, 0xba, 0x62, 0xd0, 0x46, 0x52, 0x4e, 0x45,
        0x6d, 0x2c, 0x2f, 0x3f, 0x73, 0x02, 0x20, 0x0b, 0x5f, 0x78, 0xe5,
        0x11, 0xaa, 0x18, 0x12, 0x9f, 0x6f, 0x23, 0x6d, 0x92, 0x13, 0x22,
        0x7d, 0x92, 0xb4, 0xe6, 0x7e, 0xdf, 0x53, 0xe8, 0x16, 0xdf, 0xb0,
        0x5d, 0x9d, 0xc8, 0xb9, 0x0f, 0xde
      ]));
    });
  });
  return {privateKey: keypair.privateKey, certDER: certBuilder.data};
}

/**
 * Registration encodes a registration response success message.  See "FIDO U2F
 * Raw Message Formats" (§4.3).
 */
const Registration = class {
  /**
   * @param {string} registrationData the registration response message,
   *     base64-encoded.
   * @param {string} appId the application identifier.
   * @param {string} challenge the server-generated challenge parameter. This
   *     is only used if opt_clientData is null and, in that case, is expected
   *     to be a webSafeBase64-encoded, 32-byte value.
   * @param {string=} opt_clientData the client data, base64-encoded.
   * @throws {Error}
   */
  constructor(registrationData, appId, challenge, opt_clientData) {
    var data = new ByteString(decodeWebSafeBase64ToArray(registrationData));
    var magic = data.getBytes(1);
    if (magic[0] != 5) {
      throw Error('bad magic number');
    }
    /** @private {!Uint8Array} */
    this.publicKey_ = data.getBytes(65);
    /** @private {!Uint8Array} */
    this.keyHandleLen_ = data.getBytes(1);
    /** @private {!Uint8Array} */
    this.keyHandle_ = data.getBytes(this.keyHandleLen_[0]);
    /** @private {!Uint8Array} */
    this.certificate_ = data.getASN1Element(Tag.SEQUENCE).data;
    /** @private {!Uint8Array} */
    this.signature_ = data.getASN1Element(Tag.SEQUENCE).data;
    if (!data.empty) {
      throw Error('extra trailing bytes');
    }

    var challengeHash;
    if (!opt_clientData) {
      // U2F_V1 - deprecated
      challengeHash = decodeWebSafeBase64ToArray(challenge);
      if (challengeHash.length != 32) {
        throw Error('bad challenge length for U2F_V1');
      }
    } else {
      // U2F_V2
      challengeHash =
          sha256HashOfString(atob(webSafeBase64ToNormal(opt_clientData)));
    }

    /** @private {string} */
    this.challengeHash_ = challengeHash;

    /** @private {string} */
    this.appId_ = appId;
  }

  /** @return {!Uint8Array} the attestation certificate, DER-encoded. */
  get certificate() {
    return this.certificate_;
  }

  /** @return {!Uint8Array} the attestation signature, DER-encoded. */
  get signature() {
    return this.signature_;
  }

  /**
   * toBeSigned marshals the parts of a registration that are signed by the
   * attestation key, however obtained.
   *
   * @return {!Uint8Array} data to be signed.
   */
  toBeSigned() {
    var tbs = new ByteBuilder();
    tbs.addBytesFromString('\0');
    tbs.addBytes(sha256HashOfString(this.appId_));
    tbs.addBytes(this.challengeHash_);
    tbs.addBytes(this.keyHandle_);
    tbs.addBytes(this.publicKey_);
    return tbs.data;
  }

  /**
   * sign signs data from the registration (see toBeSigned()) using the supplied
   * private key.  This is used in |RANDOMIZE| mode.
   *
   * @param {!webCrypto.CryptoKey} key ECDSA P-256 signing key in WebCrypto
   *     format
   * @return {Promise<!Uint8Array>} ASN.1 DER encoded ECDSA signature.
   */
  async sign(key) {
    const algo = {name: 'ECDSA', hash: {name: 'SHA-256'}};
    var signatureBuf = await crypto.subtle.sign(algo, key, this.toBeSigned());
    var signatureRaw = new ByteString(new Uint8Array(signatureBuf));
    var signatureASN1 = new ByteBuilder();
    signatureASN1.addASN1(Tag.SEQUENCE, (b) => {
      // The P-256 signature from WebCrypto is a pair of 32-byte, big-endian
      // values concatenated.
      b.addASN1BigInt(signatureRaw.getBytes(32));
      b.addASN1BigInt(signatureRaw.getBytes(32));
    });
    return signatureASN1.data;
  }

  /**
   * withReplacement marshals the registration (to base64) with the certificate
   * and signature replaced.
   *
   * @param {!Uint8Array} certificate new certificate, as DER.
   * @param {!Uint8Array} signature new signature, as DER.
   * @return {string} The supplied registration data with certificate and
   *     signature replaced, base64.
   */
  withReplacement(certificate, signature) {
    var result = new ByteBuilder();
    result.addBytesFromString('\x05');
    result.addBytes(this.publicKey_);
    result.addBytes(this.keyHandleLen_);
    result.addBytes(this.keyHandle_);
    result.addBytes(certificate);
    result.addBytes(signature);
    return B64_encode(result.data);
  }
};

/**
 * ConveyancePreference describes how to alter (if at all) the attestation
 * certificate in a registration response.
 * @enum
 */
var ConveyancePreference = {
  /**
   * NONE means that the token's attestation certificate should be replaced with
   * a randomly generated one, and that response should be re-signed using a
   * corresponding key.
   */
  NONE: 1,
  /**
   * DIRECT means that the token's attestation cert should be returned unchanged
   * to the relying party.
   */
  DIRECT: 0,
};

/**
 * conveyancePreference returns the attestation certificate replacement mode.
 *
 * @param {EnrollChallenge} enrollChallenge
 * @return {ConveyancePreference}
 */
function conveyancePreference(enrollChallenge) {
  if (enrollChallenge.hasOwnProperty('attestation') &&
      (enrollChallenge['attestation'] == 'direct' ||
       enrollChallenge['attestation'] == 'indirect')) {
    return ConveyancePreference.DIRECT;
  }
  return ConveyancePreference.NONE;
}

/**
 * Handles a U2F enroll request.
 * @param {MessageSender} messageSender The message sender.
 * @param {Object} request The web page's enroll request.
 * @param {Function} sendResponse Called back with the result of the enroll.
 * @return {Closeable} A handler object to be closed when the browser channel
 *     closes.
 */
function handleU2fEnrollRequest(messageSender, request, sendResponse) {
  var sentResponse = false;
  var closeable = null;

  function sendErrorResponse(error) {
    var response =
        makeU2fErrorResponse(request, error.errorCode, error.errorMessage);
    sendResponseOnce(sentResponse, closeable, response, sendResponse);
  }

  var sender = createSenderFromMessageSender(messageSender);
  if (!sender) {
    sendErrorResponse({errorCode: ErrorCodes.BAD_REQUEST});
    return null;
  }

  async function getRegistrationData(
      appId, enrollChallenge, registrationData, opt_clientData) {
    var isDirect = true;

    if (conveyancePreference(enrollChallenge) == ConveyancePreference.NONE) {
      isDirect = false;
    } else if (chrome.cryptotokenPrivate != null) {
      isDirect = await(new Promise((resolve, reject) => {
        chrome.cryptotokenPrivate.canAppIdGetAttestation(
            {'appId': appId,
             'tabId': messageSender.tab.id,
             'origin': sender.origin,
            }, resolve);
      }));
    }

    if (isDirect) {
      return registrationData;
    }

    const reg = new Registration(
        registrationData, appId, enrollChallenge['challenge'], opt_clientData);
    const keypair = await makeCertAndKey(reg.certificate);
    const signature = await reg.sign(keypair.privateKey);
    return reg.withReplacement(keypair.certDER, signature);
  }

  /**
   * @param {string} u2fVersion
   * @param {string} registrationData Registration data, base64
   * @param {string=} opt_clientData Base64.
   */
  function sendSuccessResponse(u2fVersion, registrationData, opt_clientData) {
    var enrollChallenges = request['registerRequests'];
    var enrollChallengeOrNull =
        findEnrollChallengeOfVersion(enrollChallenges, u2fVersion);
    if (!enrollChallengeOrNull) {
      sendErrorResponse({errorCode: ErrorCodes.OTHER_ERROR});
      return;
    }
    var enrollChallenge = enrollChallengeOrNull;  // Avoids compiler warning.
    var appId = request['appId'];
    if (enrollChallenge.hasOwnProperty('appId')) {
      appId = enrollChallenge['appId'];
    }

    getRegistrationData(
        appId, enrollChallenge, registrationData, opt_clientData)
        .then(
            (registrationData) => {
              var responseData = makeEnrollResponseData(
                  enrollChallenge, u2fVersion, registrationData,
                  opt_clientData);
              var response = makeU2fSuccessResponse(request, responseData);
              sendResponseOnce(sentResponse, closeable, response, sendResponse);
            },
            (err) => {
              console.warn(
                  'attestation certificate replacement failed: ' + err);
              sendErrorResponse({errorCode: ErrorCodes.OTHER_ERROR});
            });
  }

  function timeout() {
    sendErrorResponse({errorCode: ErrorCodes.TIMEOUT});
  }

  if (sender.origin.indexOf('http://') == 0 && !HTTP_ORIGINS_ALLOWED) {
    sendErrorResponse({errorCode: ErrorCodes.BAD_REQUEST});
    return null;
  }

  if (!isValidEnrollRequest(request)) {
    sendErrorResponse({errorCode: ErrorCodes.BAD_REQUEST});
    return null;
  }

  var timeoutValueSeconds = getTimeoutValueFromRequest(request);
  // Attenuate watchdog timeout value less than the enroller's timeout, so the
  // watchdog only fires after the enroller could reasonably have called back,
  // not before.
  var watchdogTimeoutValueSeconds = attenuateTimeoutInSeconds(
      timeoutValueSeconds, MINIMUM_TIMEOUT_ATTENUATION_SECONDS / 2);
  var watchdog =
      new WatchdogRequestHandler(watchdogTimeoutValueSeconds, timeout);
  var wrappedErrorCb = watchdog.wrapCallback(sendErrorResponse);
  var wrappedSuccessCb = watchdog.wrapCallback(sendSuccessResponse);
  // TODO: Fix unused; intended to pass wrapped callbacks to Enroller?

  var timer = createAttenuatedTimer(
      FACTORY_REGISTRY.getCountdownFactory(), timeoutValueSeconds);
  var logMsgUrl = request['logMsgUrl'];
  var enroller = new Enroller(
      timer, sender, sendErrorResponse, sendSuccessResponse, logMsgUrl);
  watchdog.setCloseable(/** @type {!Closeable} */ (enroller));
  closeable = watchdog;

  var registerRequests = request['registerRequests'];
  var signRequests = getSignRequestsFromEnrollRequest(request);
  enroller.doEnroll(registerRequests, signRequests, request['appId']);

  return closeable;
}

/**
 * Returns whether the request appears to be a valid enroll request.
 * @param {Object} request The request.
 * @return {boolean} Whether the request appears valid.
 */
function isValidEnrollRequest(request) {
  if (!request.hasOwnProperty('registerRequests'))
    return false;
  var enrollChallenges = request['registerRequests'];
  if (!enrollChallenges.length)
    return false;
  var hasAppId = request.hasOwnProperty('appId');
  if (!isValidEnrollChallengeArray(enrollChallenges, !hasAppId))
    return false;
  var signChallenges = getSignChallenges(request);
  // A missing sign challenge array is ok, in the case the user is not already
  // enrolled.
  // A challenge value need not necessarily be supplied with every challenge.
  var challengeRequired = false;
  if (signChallenges &&
      !isValidSignChallengeArray(signChallenges, challengeRequired, !hasAppId))
    return false;
  return true;
}

/**
 * @typedef {{
 *   version: (string|undefined),
 *   challenge: string,
 *   appId: string
 * }}
 */
var EnrollChallenge;

/**
 * @param {Array<EnrollChallenge>} enrollChallenges The enroll challenges to
 *     validate.
 * @param {boolean} appIdRequired Whether the appId property is required on
 *     each challenge.
 * @return {boolean} Whether the given array of challenges is a valid enroll
 *     challenges array.
 */
function isValidEnrollChallengeArray(enrollChallenges, appIdRequired) {
  var seenVersions = {};
  for (var i = 0; i < enrollChallenges.length; i++) {
    var enrollChallenge = enrollChallenges[i];
    var version = enrollChallenge['version'];
    if (!version) {
      // Version is implicitly V1 if not specified.
      version = 'U2F_V1';
    }
    if (version != 'U2F_V1' && version != 'U2F_V2') {
      return false;
    }
    if (seenVersions[version]) {
      // Each version can appear at most once.
      return false;
    }
    seenVersions[version] = version;
    if (appIdRequired && !enrollChallenge['appId']) {
      return false;
    }
    if (!enrollChallenge['challenge']) {
      // The challenge is required.
      return false;
    }
  }
  return true;
}

/**
 * Finds the enroll challenge of the given version in the enroll challenge
 * array.
 * @param {Array<EnrollChallenge>} enrollChallenges The enroll challenges to
 *     search.
 * @param {string} version Version to search for.
 * @return {?EnrollChallenge} The enroll challenge with the given versions, or
 *     null if it isn't found.
 */
function findEnrollChallengeOfVersion(enrollChallenges, version) {
  for (var i = 0; i < enrollChallenges.length; i++) {
    if (enrollChallenges[i]['version'] == version) {
      return enrollChallenges[i];
    }
  }
  return null;
}

/**
 * Makes a responseData object for the enroll request with the given parameters.
 * @param {EnrollChallenge} enrollChallenge The enroll challenge used to
 *     register.
 * @param {string} u2fVersion Version of gnubby that enrolled.
 * @param {string} registrationData The registration data.
 * @param {string=} opt_clientData The client data, if available.
 * @return {Object} The responseData object.
 */
function makeEnrollResponseData(
    enrollChallenge, u2fVersion, registrationData, opt_clientData) {
  var responseData = {};
  responseData['registrationData'] = registrationData;
  // Echo the used challenge back in the reply.
  for (var k in enrollChallenge) {
    responseData[k] = enrollChallenge[k];
  }
  if (u2fVersion == 'U2F_V2') {
    // For U2F_V2, the challenge sent to the gnubby is modified to be the
    // hash of the client data. Include the client data.
    responseData['clientData'] = opt_clientData;
  }
  return responseData;
}

/**
 * Gets the expanded sign challenges from an enroll request, potentially by
 * modifying the request to contain a challenge value where one was omitted.
 * (For enrolling, the server isn't interested in the value of a signature,
 * only whether the presented key handle is already enrolled.)
 * @param {Object} request The request.
 * @return {Array<SignChallenge>}
 */
function getSignRequestsFromEnrollRequest(request) {
  var signChallenges;
  if (request.hasOwnProperty('registeredKeys')) {
    signChallenges = request['registeredKeys'];
  } else {
    signChallenges = request['signRequests'];
  }
  if (signChallenges) {
    for (var i = 0; i < signChallenges.length; i++) {
      // Make sure each sign challenge has a challenge value.
      // The actual value doesn't matter, as long as it's a string.
      if (!signChallenges[i].hasOwnProperty('challenge')) {
        signChallenges[i]['challenge'] = '';
      }
    }
  }
  return signChallenges;
}

/**
 * Creates a new object to track enrolling with a gnubby.
 * @param {!Countdown} timer Timer for enroll request.
 * @param {!WebRequestSender} sender The sender of the request.
 * @param {function(U2fError)} errorCb Called upon enroll failure.
 * @param {function(string, string, (string|undefined))} successCb Called upon
 *     enroll success with the version of the succeeding gnubby, the enroll
 *     data, and optionally the browser data associated with the enrollment.
 * @param {string=} opt_logMsgUrl The url to post log messages to.
 * @constructor
 */
function Enroller(timer, sender, errorCb, successCb, opt_logMsgUrl) {
  /** @private {Countdown} */
  this.timer_ = timer;
  /** @private {WebRequestSender} */
  this.sender_ = sender;
  /** @private {function(U2fError)} */
  this.errorCb_ = errorCb;
  /** @private {function(string, string, (string|undefined))} */
  this.successCb_ = successCb;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;

  /** @private {boolean} */
  this.done_ = false;

  /** @private {Object<string, string>} */
  this.browserData_ = {};
  /** @private {Array<EnrollHelperChallenge>} */
  this.encodedEnrollChallenges_ = [];
  /** @private {Array<SignHelperChallenge>} */
  this.encodedSignChallenges_ = [];
  // Allow http appIds for http origins. (Broken, but the caller deserves
  // what they get.)
  /** @private {boolean} */
  this.allowHttp_ =
      this.sender_.origin ? this.sender_.origin.indexOf('http://') == 0 : false;
  /** @private {RequestHandler} */
  this.handler_ = null;
}

/**
 * Default timeout value in case the caller never provides a valid timeout.
 */
Enroller.DEFAULT_TIMEOUT_MILLIS = 30 * 1000;

/**
 * Performs an enroll request with the given enroll and sign challenges.
 * @param {Array<EnrollChallenge>} enrollChallenges A set of enroll challenges.
 * @param {Array<SignChallenge>} signChallenges A set of sign challenges for
 *     existing enrollments for this user and appId.
 * @param {string=} opt_appId The app id for the entire request.
 */
Enroller.prototype.doEnroll = function(
    enrollChallenges, signChallenges, opt_appId) {
  /** @private {Array<EnrollChallenge>} */
  this.enrollChallenges_ = enrollChallenges;
  /** @private {Array<SignChallenge>} */
  this.signChallenges_ = signChallenges;
  /** @private {(string|undefined)} */
  this.appId_ = opt_appId;
  var self = this;
  getTabIdWhenPossible(this.sender_)
      .then(
          function() {
            if (self.done_)
              return;
            self.approveOrigin_();
          },
          function() {
            self.close();
            self.notifyError_({errorCode: ErrorCodes.BAD_REQUEST});
          });
};

/**
 * Ensures the user has approved this origin to use security keys, sending
 * to the request to the handler if/when the user has done so.
 * @private
 */
Enroller.prototype.approveOrigin_ = function() {
  var self = this;
  FACTORY_REGISTRY.getApprovedOrigins()
      .isApprovedOrigin(this.sender_.origin, this.sender_.tabId)
      .then(function(result) {
        if (self.done_)
          return;
        if (!result) {
          // Origin not approved: rather than give an explicit indication to
          // the web page, let a timeout occur.
          // NOTE: if you are looking at this in a debugger, this line will
          // always be false since the origin of the debugger is different
          // than origin of requesting page
          if (self.timer_.expired()) {
            self.notifyTimeout_();
            return;
          }
          var newTimer = self.timer_.clone(self.notifyTimeout_.bind(self));
          self.timer_.clearTimeout();
          self.timer_ = newTimer;
          return;
        }
        self.sendEnrollRequestToHelper_();
      });
};

/**
 * Notifies the caller of a timeout error.
 * @private
 */
Enroller.prototype.notifyTimeout_ = function() {
  this.notifyError_({errorCode: ErrorCodes.TIMEOUT});
};

/**
 * Performs an enroll request with this instance's enroll and sign challenges,
 * by encoding them into a helper request and passing the resulting request to
 * the factory registry's helper.
 * @private
 */
Enroller.prototype.sendEnrollRequestToHelper_ = function() {
  var encodedEnrollChallenges =
      this.encodeEnrollChallenges_(this.enrollChallenges_, this.appId_);
  // If the request didn't contain a sign challenge, provide one. The value
  // doesn't matter.
  var defaultSignChallenge = '';
  var encodedSignChallenges = encodeSignChallenges(
      this.signChallenges_, defaultSignChallenge, this.appId_);
  var request = {
    type: 'enroll_helper_request',
    enrollChallenges: encodedEnrollChallenges,
    signData: encodedSignChallenges,
    logMsgUrl: this.logMsgUrl_
  };
  if (!this.timer_.expired()) {
    request.timeout = this.timer_.millisecondsUntilExpired() / 1000.0;
    request.timeoutSeconds = this.timer_.millisecondsUntilExpired() / 1000.0;
  }

  // Begin fetching/checking the app ids.
  var enrollAppIds = [];
  if (this.appId_) {
    enrollAppIds.push(this.appId_);
  }
  for (var i = 0; i < this.enrollChallenges_.length; i++) {
    if (this.enrollChallenges_[i].hasOwnProperty('appId')) {
      enrollAppIds.push(this.enrollChallenges_[i]['appId']);
    }
  }
  // Sanity check
  if (!enrollAppIds.length) {
    console.warn(UTIL_fmt('empty enroll app ids?'));
    this.notifyError_({errorCode: ErrorCodes.BAD_REQUEST});
    return;
  }
  var self = this;
  this.checkAppIds_(enrollAppIds, function(result) {
    if (self.done_)
      return;
    if (result) {
      self.handler_ = FACTORY_REGISTRY.getRequestHelper().getHandler(request);
      if (self.handler_) {
        var helperComplete =
            /** @type {function(HelperReply)} */
            (self.helperComplete_.bind(self));
        self.handler_.run(helperComplete);
      } else {
        self.notifyError_({errorCode: ErrorCodes.OTHER_ERROR});
      }
    } else {
      self.notifyError_({errorCode: ErrorCodes.BAD_REQUEST});
    }
  });
};

/**
 * Encodes the enroll challenge as an enroll helper challenge.
 * @param {EnrollChallenge} enrollChallenge The enroll challenge to encode.
 * @param {string=} opt_appId The app id for the entire request.
 * @return {EnrollHelperChallenge} The encoded challenge.
 * @private
 */
Enroller.encodeEnrollChallenge_ = function(enrollChallenge, opt_appId) {
  var encodedChallenge = {};
  var version;
  if (enrollChallenge['version']) {
    version = enrollChallenge['version'];
  } else {
    // Version is implicitly V1 if not specified.
    version = 'U2F_V1';
  }
  encodedChallenge['version'] = version;
  encodedChallenge['challengeHash'] = enrollChallenge['challenge'];
  var appId;
  if (enrollChallenge['appId']) {
    appId = enrollChallenge['appId'];
  } else {
    appId = opt_appId;
  }
  if (!appId) {
    // Sanity check. (Other code should fail if it's not set.)
    console.warn(UTIL_fmt('No appId?'));
  }
  encodedChallenge['appIdHash'] = B64_encode(sha256HashOfString(appId));
  return /** @type {EnrollHelperChallenge} */ (encodedChallenge);
};

/**
 * Encodes the given enroll challenges using this enroller's state.
 * @param {Array<EnrollChallenge>} enrollChallenges The enroll challenges.
 * @param {string=} opt_appId The app id for the entire request.
 * @return {!Array<EnrollHelperChallenge>} The encoded enroll challenges.
 * @private
 */
Enroller.prototype.encodeEnrollChallenges_ = function(
    enrollChallenges, opt_appId) {
  var challenges = [];
  for (var i = 0; i < enrollChallenges.length; i++) {
    var enrollChallenge = enrollChallenges[i];
    var version = enrollChallenge.version;
    if (!version) {
      // Version is implicitly V1 if not specified.
      version = 'U2F_V1';
    }

    if (version == 'U2F_V2') {
      var modifiedChallenge = {};
      for (var k in enrollChallenge) {
        modifiedChallenge[k] = enrollChallenge[k];
      }
      // V2 enroll responses contain signatures over a browser data object,
      // which we're constructing here. The browser data object contains, among
      // other things, the server challenge.
      var serverChallenge = enrollChallenge['challenge'];
      var browserData = makeEnrollBrowserData(
          serverChallenge, this.sender_.origin, this.sender_.tlsChannelId);
      // Replace the challenge with the hash of the browser data.
      modifiedChallenge['challenge'] =
          B64_encode(sha256HashOfString(browserData));
      this.browserData_[version] = B64_encode(UTIL_StringToBytes(browserData));
      challenges.push(Enroller.encodeEnrollChallenge_(
          /** @type {EnrollChallenge} */ (modifiedChallenge), opt_appId));
    } else {
      challenges.push(
          Enroller.encodeEnrollChallenge_(enrollChallenge, opt_appId));
    }
  }
  return challenges;
};

/**
 * Checks the app ids associated with this enroll request, and calls a callback
 * with the result of the check.
 * @param {!Array<string>} enrollAppIds The app ids in the enroll challenge
 *     portion of the enroll request.
 * @param {function(boolean)} cb Called with the result of the check.
 * @private
 */
Enroller.prototype.checkAppIds_ = function(enrollAppIds, cb) {
  var appIds =
      UTIL_unionArrays(enrollAppIds, getDistinctAppIds(this.signChallenges_));
  FACTORY_REGISTRY.getOriginChecker()
      .canClaimAppIds(this.sender_.origin, appIds)
      .then(this.originChecked_.bind(this, appIds, cb));
};

/**
 * Called with the result of checking the origin. When the origin is allowed
 * to claim the app ids, begins checking whether the app ids also list the
 * origin.
 * @param {!Array<string>} appIds The app ids.
 * @param {function(boolean)} cb Called with the result of the check.
 * @param {boolean} result Whether the origin could claim the app ids.
 * @private
 */
Enroller.prototype.originChecked_ = function(appIds, cb, result) {
  if (!result) {
    this.notifyError_({errorCode: ErrorCodes.BAD_REQUEST});
    return;
  }
  var appIdChecker = FACTORY_REGISTRY.getAppIdCheckerFactory().create();
  appIdChecker
      .checkAppIds(
          this.timer_.clone(), this.sender_.origin, appIds, this.allowHttp_,
          this.logMsgUrl_)
      .then(cb);
};

/** Closes this enroller. */
Enroller.prototype.close = function() {
  if (this.handler_) {
    this.handler_.close();
    this.handler_ = null;
  }
  this.done_ = true;
};

/**
 * Notifies the caller with the error.
 * @param {U2fError} error Error.
 * @private
 */
Enroller.prototype.notifyError_ = function(error) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.errorCb_(error);
};

/**
 * Notifies the caller of success with the provided response data.
 * @param {string} u2fVersion Protocol version
 * @param {string} info Response data
 * @param {string=} opt_browserData Browser data used
 * @private
 */
Enroller.prototype.notifySuccess_ = function(
    u2fVersion, info, opt_browserData) {
  if (this.done_)
    return;
  this.close();
  this.done_ = true;
  this.successCb_(u2fVersion, info, opt_browserData);
};

/**
 * Called by the helper upon completion.
 * @param {EnrollHelperReply} reply The result of the enroll request.
 * @private
 */
Enroller.prototype.helperComplete_ = function(reply) {
  if (reply.code) {
    var reportedError = mapDeviceStatusCodeToU2fError(reply.code);
    console.log(UTIL_fmt(
        'helper reported ' + reply.code.toString(16) + ', returning ' +
        reportedError.errorCode));
    // Log non-expected reply codes if we have url to send them.
    if (reportedError.errorCode == ErrorCodes.OTHER_ERROR) {
      var logMsg = 'log=u2fenroll&rc=' + reply.code.toString(16);
      if (this.logMsgUrl_)
        logMessage(logMsg, this.logMsgUrl_);
    }
    this.notifyError_(reportedError);
  } else {
    console.log(UTIL_fmt('Gnubby enrollment succeeded!!!!!'));
    var browserData;

    if (reply.version == 'U2F_V2') {
      // For U2F_V2, the challenge sent to the gnubby is modified to be the hash
      // of the browser data. Include the browser data.
      browserData = this.browserData_[reply.version];
    }

    this.notifySuccess_(
        /** @type {string} */ (reply.version),
        /** @type {string} */ (reply.enrollData), browserData);
  }
};
