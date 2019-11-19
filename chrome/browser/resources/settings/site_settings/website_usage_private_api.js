// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

Polymer({
  is: 'website-usage-private-api',

  properties: {
    /**
     * The amount of data used by the given website.
     */
    websiteDataUsage: {
      type: String,
      notify: true,
    },

    /**
     * The number of cookies used by the given website.
     */
    websiteCookieUsage: {
      type: String,
      notify: true,
    },
  },

  /** @override */
  attached: function() {
    settings.WebsiteUsagePrivateApi.websiteUsagePolymerInstance = this;
  },

  /** @param {string} host */
  fetchUsageTotal: function(host) {
    settings.WebsiteUsagePrivateApi.fetchUsageTotal(host);
  },

  /**
   * @param {string} origin
   */
  clearUsage: function(origin) {
    settings.WebsiteUsagePrivateApi.clearUsage(origin);
  },

  /** @param {string} origin */
  notifyUsageDeleted: function(origin) {
    this.fire('usage-deleted', {origin: origin});
  },
});
})();

cr.define('settings.WebsiteUsagePrivateApi', function() {
  /**
   * @type {Object} An instance of the polymer object defined above.
   * All data will be set here.
   */
  const websiteUsagePolymerInstance = null;

  /**
   * @type {string} The host for which the usage total is being fetched.
   */
  let hostName;

  /**
   * Encapsulates the calls between JS and C++ to fetch how much storage the
   * host is using.
   * Will update the data in |websiteUsagePolymerInstance|.
   */
  const fetchUsageTotal = function(host) {
    const instance =
        settings.WebsiteUsagePrivateApi.websiteUsagePolymerInstance;
    if (instance != null) {
      instance.websiteDataUsage = '';
    }

    hostName = host;
    chrome.send('fetchUsageTotal', [host]);
  };

  /**
   * Callback for when the usage total is known.
   * @param {string} host The host that the usage was fetched for.
   * @param {string} usage The string showing how much data the given host
   *     is using.
   */
  const returnUsageTotal = function(host, usage, cookies) {
    const instance =
        settings.WebsiteUsagePrivateApi.websiteUsagePolymerInstance;
    if (instance == null) {
      return;
    }

    if (hostName == host) {
      instance.websiteDataUsage = usage;
      instance.websiteCookieUsage = cookies;
    }
  };

  /**
   * Deletes the storage being used for a given origin.
   * @param {string} origin The origin to delete storage for.
   */
  const clearUsage = function(origin) {
    chrome.send('clearUsage', [origin]);
    const instance =
        settings.WebsiteUsagePrivateApi.websiteUsagePolymerInstance;
    if (instance == null) {
      return;
    }

    instance.notifyUsageDeleted(origin);
  };

  return {
    websiteUsagePolymerInstance: websiteUsagePolymerInstance,
    fetchUsageTotal: fetchUsageTotal,
    returnUsageTotal: returnUsageTotal,
    clearUsage: clearUsage,
  };
});
