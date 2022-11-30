// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="chromeos_ash">
import {appendParam} from 'chrome://resources/ash/common/util.js';
// </if>
// <if expr="not chromeos_ash">
import {appendParam} from 'chrome://resources/js/util_ts.js';
// </if>

/**
 * Try to autofill email on login page for supported identity providers
 * @param {string} url Url of IdP login page
 * @param {?string} urlParameterNameToAutofillUsername Url parameter name
 *     which can be used to autofill the username field
 * @param {?string} email User's email which is to be used as a username on
 *     IdP login page
 * @return {?string} Modified url which can autofill the username field, or
 *     null.
 */
export function maybeAutofillUsername(
    url, urlParameterNameToAutofillUsername, email) {
  if (!urlParameterNameToAutofillUsername ||
      urlParameterNameToAutofillUsername.length === 0) {
    return null;
  }
  if (!url.startsWith('https')) {
    return null;
  }
  if (!email) {
    return null;
  }
  // Don't do anything if url already contains
  // `urlParameterNameToAutofillUsername`.
  if (url.match(urlParameterNameToAutofillUsername)) {
    return null;
  }

  url = appendParam(url, urlParameterNameToAutofillUsername, email);
  return url;
}
