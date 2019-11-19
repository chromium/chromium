// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="saml_timestamps.js">

/**
 * @fileoverview A utility for extracting password information from SAML
 * authorization response. This requires that the SAML IDP administrator
 * has correctly configured their domain to issue these attributes.
 */

cr.define('samlPasswordAttributes', function() {
  'use strict';

  /** @const @private {number} The shortest XML string that could be useful. */
  const MIN_SANE_XML_LENGTH = 100;

  /** @const @private {number} The max length that we are willing to parse. */
  const MAX_SANE_XML_LENGTH = 50 * 1024;  // 50 KB

  /** @const @private {string} Schema name prefix. */
  const SCHEMA_NAME_PREFIX = 'http://schemas.google.com/saml/2019/';

  /** @const @private {string} Schema name for password modified timestamp. */
  const PASSWORD_MODIFIED_TIMESTAMP = 'passwordmodifiedtimestamp';

  /** @const @private {string} Schema name for password expiration timestamp. */
  const PASSWORD_EXPIRATION_TIMESTAMP = 'passwordexpirationtimestamp';

  /** @const @private {string} Schema name for password-change URL. */
  const PASSWORD_CHANGE_URL = 'passwordchangeurl';

  /**
   * Query selector for finding an element with tag AttributeValue that is a
   * child of an element of tag Attribute with a certain Name attribute.
   * @const @private {string}
   */
  const QUERY_SELECTOR_FORMAT = 'Attribute[Name="{0}"] > AttributeValue';

  /** Turns a schema name into a query selector to find the AttributeValue. */
  function makeQuerySelector(schemaName) {
    return QUERY_SELECTOR_FORMAT.replace(
        '{0}', SCHEMA_NAME_PREFIX + schemaName);
  }

  /** @const @private {string} Query selector for password modified time. */
  const PASSWORD_MODIFIED_TIMESTAMP_SELECTOR =
      makeQuerySelector(PASSWORD_MODIFIED_TIMESTAMP);

  /** @const @private {string} Query selector for password expiration time. */
  const PASSWORD_EXPIRATION_TIMESTAMP_SELECTOR =
      makeQuerySelector(PASSWORD_EXPIRATION_TIMESTAMP);

  /** @const @private {string} Query selector for password expiration time. */
  const PASSWORD_CHANGE_URL_SELECTOR = makeQuerySelector(PASSWORD_CHANGE_URL);

  /**
   * Extract password information from the Attribute elements in the given SAML
   * authorization response.
   * @param {string} xmlStr The SAML response XML, as a string.
   * @return {!PasswordAttributes} A struct containing all the attributes that
   * could be extracted, formatted as strings. Some or all of the strings can
   * be empty if some or all of the attributes could not be extracted.
   */
  function readPasswordAttributes(xmlStr) {
    // Don't throw any exception that could cause login to fail - extracting
    // these attributes can fail, but login should not be interrupted.
    try {
      if (!xmlStr || typeof xmlStr != 'string') {
        return PasswordAttributes.EMPTY;
      }
      if (xmlStr.length < MIN_SANE_XML_LENGTH ||
          xmlStr.length > MAX_SANE_XML_LENGTH) {
        return PasswordAttributes.EMPTY;
      }
      if (!xmlStr.includes(SCHEMA_NAME_PREFIX)) {
        // No need to bother parsing the XML if it doesn't contain this string.
        return PasswordAttributes.EMPTY;
      }

      const xmlDom = new DOMParser().parseFromString(xmlStr, 'text/xml');
      if (!xmlDom) {
        return PasswordAttributes.EMPTY;
      }

      return new PasswordAttributes(
          extractTimestampFromXml(xmlDom, PASSWORD_MODIFIED_TIMESTAMP_SELECTOR),
          extractTimestampFromXml(
              xmlDom, PASSWORD_EXPIRATION_TIMESTAMP_SELECTOR),
          extractStringFromXml(xmlDom, PASSWORD_CHANGE_URL_SELECTOR));

    } catch (error) {
      console.error('Error reading password attributes: ' + error);
      return PasswordAttributes.EMPTY;
    }
  }

  /**
   * Extracts a string from the given XML DOM, using the given query selector.
   * @param {!XMLDocument} xmlDom The XML DOM.
   * @param {string} querySelectorStr The query selector to find the string.
   * @return {string} The extracted string (empty if failed to extract).
   */
  function extractStringFromXml(xmlDom, querySelectorStr) {
    const element = xmlDom.querySelector(querySelectorStr);
    return (element && element.textContent) ? element.textContent : '';
  }

  /**
   * Extracts a timestamp from the given XML DOM, using the given query selector
   * to find it and using {@code samlTimestamps.decodeTimestamp} to decode it.
   * @param {!XMLDocument} xmlDom The XML DOM.
   * @param {string} querySelectorStr The query selector to find the timestamp.
   * @return {string} The timestamp as number of ms since 1970, formatted as a
   * string (or an empty string if the timestamp could not be extracted).
   */
  function extractTimestampFromXml(xmlDom, querySelectorStr) {
    const valueText = extractStringFromXml(xmlDom, querySelectorStr);
    if (!valueText) return '';

    const timestamp = samlTimestamps.decodeTimestamp(valueText);
    return timestamp ? String(timestamp.valueOf()) : '';
  }

  /**
   * Immutable struct to hold password attributes. All three fields are strings
   * and are always present, but they are empty if that information is missing.
   * Timestamps are in JS time - the number of ms since 1 January 1970 - but
   * are also formatted as strings, since this struct is sent from JS into C++,
   * and strings travel easier than int64s across this boundary.
   * If this struct is changed, SamlPasswordAttributes::FromJS in
   * saml_password_attributes.cc must also be changed.
   * @export @final
   */
  class PasswordAttributes {
    constructor(modifiedTime, expirationTime, passwordChangeUrl) {
      /** @type {string} Password last-modified timestamp. */
      this.modifiedTime = modifiedTime;

      /** @type {string} Password expiration timestamp. */
      this.expirationTime = expirationTime;

      /** @type {string} Password-change URL. */
      this.passwordChangeUrl = passwordChangeUrl;

      Object.freeze(this);  // Make immutable.
    }
  }

  /** An immutable and empty PasswordAttributes struct. */
  PasswordAttributes.EMPTY = new PasswordAttributes('', '', '');

  // Public functions:
  return {
    readPasswordAttributes: readPasswordAttributes,
    PasswordAttributes: PasswordAttributes,
  };
});
