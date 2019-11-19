// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A utility for decoding timestamps strings into JS Date objects.
 * This is needed for showing the SAML password expiry notifications.
 * Timestamps are allowed to be sent to us in a variety of formats, since SAML
 * administrators may not have the ability to convert between formats at their
 * end. This class doesn't need to be informed which format the timestamp is in,
 * since the different allowed formats don't tend to overlap in practice.
 *
 * The supported formats are NTFS filetimes, Unix time (in seconds or ms),
 * and ISO 8601.
 */

cr.define('samlTimestamps', function() {
  'use strict';

  /** @const @private {number} Maximum length of a valid timestamp. */
  const MAX_SANE_LENGTH = 30;

  /** @const @private {!Date} The earliest date considered sane. */
  const MIN_SANE_DATE = new Date('1980-01-01 UTC');

  /** @const @private {!Date} The latest date considered sane. */
  const MAX_SANE_DATE = new Date('10000-01-01 UTC');

  /** @const @private {!Date} Epoch for Windows NTFS FILETIME timestamps. */
  const NTFS_EPOCH = new Date('1601-01-01 UTC');

  /** @const @private {!RegExp} Pattern to match integers. */
  const INTEGER_PATTERN = /^-?\d+$/;

  /**
   * Pattern to match ISO 8601 dates / times. Rejects other text-based timestamp
   * formats (eg '01-02-03') since they cannot be parsed in a consistent way.
   * @const @private {!RegExp}
   */
  const ISO_8601_PATTERN = /^\d\d\d\d-\d\d-\d\d(T|$)/;

  /**
   * Decode a timestamp string that is in one of the supported formats.
   * @param {string} str A timestamp formatted as a string.
   * @return {?Date} A valid decoded timestamp, or null.
   */
  function decodeTimestamp(str) {
    str = str.trim();
    if (str.length == 0 || str.length > MAX_SANE_LENGTH) {
      return null;
    }

    if (INTEGER_PATTERN.test(str)) {
      return decodeIntegerTimestamp(parseInt(str));
    } else if (ISO_8601_PATTERN.test(str)) {
      return decodeIso8601(str);
    }
    return null;
  }

  /**
   * Decode a timestamp that is in one of the supported integer formats:
   * NTFS filetime, Unix time (s), or Unix time (ms).
   * @param {number} num An integer timestamp.
   * @return {?Date} A valid decoded timestamp, or null.
   */
  function decodeIntegerTimestamp(num) {
    // We don't ask which format integer timestamps are in, because we can guess
    // confidently by choosing the decode function that gives a sane result.
    let result;
    for (let decodeFunc of [decodeNtfsFiletime,
                            decodeUnixMilliseconds,
                            decodeUnixSeconds]) {
      result = decodeFunc(num);
      if (result && result >= MIN_SANE_DATE && result <= MAX_SANE_DATE) {
        return result;
      }
    }
    // For dates that fall outside the sane range, we cannot guess which format
    // was used, but at least we can tell if the result should be in the far
    // past or the far future, and return a result that is roughly equivalent.
    return result && result < MIN_SANE_DATE
          ? new Date(MIN_SANE_DATE)   // Copy-before-return protects these two
          : new Date(MAX_SANE_DATE);  // constants (since Date is mutable).
  }

  /**
   * Decode a NTFS filetime timestamp with an epoch of year 1601.
   * @param {number} hundredsOfNs Each tick measures 100 nanoseconds.
   * @return {?Date}
   */
  function decodeNtfsFiletime(hundredsOfNs) {
    return createValidDate(NTFS_EPOCH.valueOf() + (hundredsOfNs / 10000));
  }

  /**
   * Decode a Unix timestamp which is counting milliseconds since 1970.
   * @param {number} milliseconds
   * @return {?Date}
   */
  function decodeUnixMilliseconds(milliseconds) {
    return createValidDate(milliseconds);
  }

  /**
   * Decode a Unix timestamp which is counting seconds since 1970.
   * @param {number} seconds
   * @return {?Date}
   */
  function decodeUnixSeconds(seconds) {
    return createValidDate(seconds * 1000);
  }

  /**
   * Decodes a timestamp string that is in ISO 8601 format.
   * @param {string} str
   * @return {?Date}
   */
  function decodeIso8601(str) {
    // If no timezone is specified, appending 'Z' means we will parse as UTC.
    // (If a timezone is already specified, appending 'Z' is simply invalid.)
    // Using UTC as a default is predictable, using local time is unpredictable.
    return createValidDate(str + 'Z') || createValidDate(str);
  }

  /**
   * Constructs a date and returns it if it is valid, otherwise returns null.
   * @param {*} arg Argument for constructing the date.
   * @return {?Date} A valid date object, or null.
   */
  function createValidDate(arg) {
    const date = new Date(arg);
    return isNaN(date) ? null : date;
  }

  // Public functions:
  return {decodeTimestamp: decodeTimestamp};
});
