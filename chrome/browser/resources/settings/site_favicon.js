// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'site-favicon' is the section to display the favicon given the
 * site URL.
 */

Polymer({
  is: 'site-favicon',

  properties: {
    faviconUrl: String,
    url: String,
  },

  /** @private */
  getBackgroundImage_: function() {
    let backgroundImage = cr.icon.getFavicon('');
    if (this.faviconUrl) {
      const url = this.ensureUrlHasScheme_(this.faviconUrl);
      backgroundImage = cr.icon.getFavicon(url);
    } else if (this.url) {
      let url = this.removePatternWildcard_(this.url);
      url = this.ensureUrlHasScheme_(url);
      backgroundImage = cr.icon.getFaviconForPageURL(url || '', false);
    }
    return backgroundImage;
  },

  /**
   * Removes the wildcard prefix from a pattern string.
   * @param {string} pattern The pattern to remove the wildcard from.
   * @return {string} The resulting pattern.
   * @private
   */
  removePatternWildcard_: function(pattern) {
    if (!pattern || pattern.length === 0) {
      return pattern;
    }

    if (pattern.startsWith('http://[*.]')) {
      return pattern.replace('http://[*.]', 'http://');
    } else if (pattern.startsWith('https://[*.]')) {
      return pattern.replace('https://[*.]', 'https://');
    } else if (pattern.startsWith('[*.]')) {
      return pattern.substring(4, pattern.length);
    }
    return pattern;
  },

  /**
   * Ensures the URL has a scheme (assumes http if omitted).
   * @param {string} url The URL with or without a scheme.
   * @return {string} The URL with a scheme, or an empty string.
   * @private
   */
  ensureUrlHasScheme_: function(url) {
    if (!url || url.length === 0) {
      return url;
    }
    return url.includes('://') ? url : 'http://' + url;
  },
});
