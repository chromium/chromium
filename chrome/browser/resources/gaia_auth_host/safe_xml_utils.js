// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const */
const BEGIN_CERTIFICATE = '-----BEGIN CERTIFICATE-----';

/** @const */
const END_CERTIFICATE = '-----END CERTIFICATE-----';

let xmlPolicy = null;

/**
 * With Trusted Types Content Security Policy enabled, we cannot parseFromString
 * on untrusted content types. This class encapsulates and abstracts parsing
 * of XML strings to reduce the risk of introducing unsafe sink assignments in
 * other contexts.
 */
export class SafeXMLUtils {
  constructor(xmlString) {
    if (xmlPolicy === null) {
      // This policy is only meant to be used for extracting value out of XML.
      // Policy not to be used from outside of this file.
      xmlPolicy = window.trustedTypes.createPolicy('xml-policy', {
        createHTML: (s) => (s),
      });
    }
    const xml = xmlPolicy.createHTML(xmlString);
    const parser = new DOMParser();
    this.xmlDoc = parser.parseFromString(xml, 'text/xml');
  }

  /**
   * Return x509certificate in pem-format which is extracted from our xml
   * and will be used to record SAML provider.
   * @return {string|null} The X509Certificate if one exists, null otherwise.
   */
  getX509Certificate() {
    let certificate = this.xmlDoc.getElementsByTagName('ds:X509Certificate');
    if (!certificate || certificate.length === 0) {
      // tag 'ds:X509Certificate' doesn't exist
      certificate = this.xmlDoc.getElementsByTagName('X509Certificate');
    }

    if (certificate && certificate.length > 0 && certificate[0].childNodes &&
        certificate[0].childNodes[0] &&
        certificate[0].childNodes[0].nodeValue) {
      certificate = certificate[0].childNodes[0].nodeValue;
      return BEGIN_CERTIFICATE + '\n' + certificate.trim() + '\n' +
          END_CERTIFICATE + '\n';
    }

    return null;
  }

  /**
   * Extracts a string from the given XML DOM, using the given query selector.
   * @param {string} querySelectorStr The query selector to find the string.
   * @return {string} The extracted string (empty if failed to extract).
   */
  extractStringFromXml(querySelectorStr) {
    const element = this.xmlDoc.querySelector(querySelectorStr);
    return (element && element.textContent) ? element.textContent : '';
  }
}
