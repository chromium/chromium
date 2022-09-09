// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides an implementation of approved origins that relies
 * on the chrome.cryptotokenPrivate.requestPermission API.
 * (and only) allows google.com to use security keys.
 *
 */
'use strict';

/**
 * Allows the caller to check whether the user has approved the use of
 * security keys from an origin.
 * @constructor
 * @implements {ApprovedOrigins}
 */
function CryptoTokenApprovedOrigin() {}

/**
 * Checks whether the origin is approved to use security keys. (If not, an
 * approval prompt may be shown.)
 * @param {string} origin The origin to approve.
 * @param {number=} opt_tabId A tab id to display approval prompt in.
 *     For this implementation, the tabId is always necessary, even though
 *     the type allows undefined.
 * @return {Promise<boolean>} A promise for the result of the check.
 */
CryptoTokenApprovedOrigin.prototype.isApprovedOrigin = function(
    origin, opt_tabId) {
  return new Promise(function(resolve, reject) {
    if (opt_tabId === undefined) {
      resolve(false);
      return;
    }
    var tabId = /** @type {number} */ (opt_tabId);
    tabInForeground(tabId).then(function(result) {
      if (!result) {
        resolve(false);
        return;
      }
      if (!chrome.tabs || !chrome.tabs.get) {
        reject();
        return;
      }
      chrome.tabs.get(tabId, function(tab) {
        if (chrome.runtime.lastError) {
          resolve(false);
          return;
        }
        var tabOrigin = getOriginFromUrl(tab.url);
        resolve(tabOrigin === origin);
      });
    });
  });
};
