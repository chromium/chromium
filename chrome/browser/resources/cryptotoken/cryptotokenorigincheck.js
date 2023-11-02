// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements a check whether an origin is allowed to assert an
 * app id based on whether they share the same effective TLD + 1.
 *
 */
'use strict';

/**
 * Implements half of the app id policy: whether an origin is allowed to claim
 * an app id. For checking whether the app id also lists the origin,
 * @see AppIdChecker.
 * @implements OriginChecker
 * @constructor
 */
function CryptoTokenOriginChecker() {}

/**
 * Checks whether the origin is allowed to claim the app ids.
 * @param {string} origin The origin claiming the app id.
 * @param {!Array<string>} appIds The app ids being claimed.
 * @return {Promise<boolean>} A promise for the result of the check.
 */
CryptoTokenOriginChecker.prototype.canClaimAppIds = function(origin, appIds) {
  var appIdChecks = appIds.map(this.checkAppId_.bind(this, origin));
  return Promise.all(appIdChecks).then(function(results) {
    return results.every(function(result) {
      return result;
    });
  });
};

/**
 * Checks if a single appId can be asserted by the given origin.
 * @param {string} origin The origin.
 * @param {string} appId The appId to check
 * @return {Promise<boolean>} A promise for the result of the check
 * @private
 */
CryptoTokenOriginChecker.prototype.checkAppId_ = function(origin, appId) {
  return new Promise(function(resolve, reject) {
    if (!chrome.cryptotokenPrivate) {
      reject();
      return;
    }
    chrome.cryptotokenPrivate.canOriginAssertAppId(origin, appId, resolve);
  });
};
