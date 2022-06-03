// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Reference to the backend.
let pageHandler = null;

(function() {
function delay(ms) {
  return new Promise((resolve, reject) => setTimeout(resolve, ms));
}

/** Setting this class changes a throbber to show an error symbol. */
const FAILED_CSS_CLASS = 'failed';

/**
 * Shows a throbber with ID |id|.
 * @param {string} id The HTML id of the throbber element to reveal.
 * @param {!Promise} promiseToWaitFor The throbber won't be hidden before this
 *     promise resolves.
 * @return {!Promise} Resolves with the result of promiseToWaitFor after the
 *     minimum delay has completed.
 */
function showThrobber(id, promiseToWaitFor) {
  const delayMs = 400;
  const element = $(id);
  element.classList.remove(FAILED_CSS_CLASS);

  element.removeAttribute('hidden');
  return Promise.all([delay(delayMs), promiseToWaitFor]).then(results => {
    element.hidden = true;
    return results[1];
  });
}

/**
 * Get the current Explore Sites service parameters and fill the DOM with
 * the values.
 */
function updatePageWithProperties() {
  pageHandler.getProperties().then(function(response) {
    for (const [field, value] of Object.entries(response.properties)) {
      $(field).textContent = value;
    }
  });
}

/**
 * Removes any existing catalogs and their version_token markers from the
 * catalog.
 */
function clearCachedCatalog() {
  const id = 'clear-catalog-throbber';
  showThrobber(id, pageHandler.clearCachedExploreSitesCatalog())
      .then(success => $(id).classList.toggle(FAILED_CSS_CLASS, !success));
}

/**
 * Sets the country code on ExploreSitesService in order to override the Finch
 * and geolocation based parameter.
 */
function overrideCountryCode() {
  const id = 'country-override-throbber';
  const newCountryCode = $('country-code-input').value;
  showThrobber(id, pageHandler.overrideCountryCode(newCountryCode))
      .then(success => {
        $(id).classList.toggle(FAILED_CSS_CLASS, !success);
        updatePageWithProperties();
      });
}

/**
 * Causes the Explore Sites service to request a catalog from the network as a
 * "foreground" request.  Does not override any of the local data, so the
 * downloaded network may be unchanged.
 */
function forceNetworkRequest() {
  const id = 'network-request-throbber';
  showThrobber(id, pageHandler.forceNetworkRequest())
      .then(e => $(id).classList.toggle(FAILED_CSS_CLASS, !e.success));
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = exploreSitesInternals.mojom.PageHandler.getRemote();
  updatePageWithProperties();

  // Set up event listeners.
  $('clear-cached-catalog').onclick = clearCachedCatalog;
  $('override-country-code').onclick = overrideCountryCode;
  $('force-network-request').onclick = forceNetworkRequest;
});
})();
