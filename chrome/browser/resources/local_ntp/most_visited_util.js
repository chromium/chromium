// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for rendering most visited thumbnails and titles.
 */

/**
 * The origin of this request.
 * @const {string}
 */
const MV_DOMAIN_ORIGIN = '{{ORIGIN}}';

/**
 * Converts an RGB color number to a hex color string if valid.
 * @param {number} color A 6-digit hex RGB color code as a number.
 * @return {?string} A CSS representation of the color or null if invalid.
 */
function convertToHexColor(color) {
  // Color must be a number, finite, with no fractional part, in the correct
  // range for an RGB hex color.
  if (isFinite(color) && Math.floor(color) == color && color >= 0 &&
      color <= 0xffffff) {
    const hexColor = color.toString(16);
    // Pads with initial zeros and # (e.g. for 'ff' yields '#0000ff').
    return '#000000'.substr(0, 7 - hexColor.length) + hexColor;
  }
  return null;
}

/**
 * Validates a RGBA color component. It must be a number between 0 and 255.
 * @param {number} component An RGBA component.
 * @return {boolean} True if the component is valid.
 */
function isValidRBGAComponent(component) {
  return isFinite(component) && component >= 0 && component <= 255;
}

/**
 * Converts an Array of color components into RGBA format "rgba(R,G,B,A)".
 * @param {Array<number>} rgbaColor Array of rgba color components.
 * @return {?string} CSS color in RGBA format or null if invalid.
 */
function convertArrayToRGBAColor(rgbaColor) {
  // Array must contain 4 valid components.
  if (rgbaColor instanceof Array && rgbaColor.length === 4 &&
      isValidRBGAComponent(rgbaColor[0]) &&
      isValidRBGAComponent(rgbaColor[1]) &&
      isValidRBGAComponent(rgbaColor[2]) &&
      isValidRBGAComponent(rgbaColor[3])) {
    return 'rgba(' + rgbaColor[0] + ',' + rgbaColor[1] + ',' + rgbaColor[2] +
        ',' + rgbaColor[3] / 255 + ')';
  }
  return null;
}

/**
 * Parses query parameters from Location.
 * @param {!Location} location The URL to generate the CSS url for.
 * @return {Object} Dictionary containing name value pairs for URL.
 *
 * TODO(dbeam): we should update callers of this method to use
 * URLSearchParams#get() instead (which I have a higher confidence handles
 * escaping and edge cases correctly). Note: that calling URLSearchParams#get()
 * also has the behavior of only returning the first &param= in the URL (i.e.
 * ?param=1&param=2 + .get('param') would return '1').
 */
function parseQueryParams(location) {
  const params = Object.create(null);
  const query = location.search.substring(1);
  const vars = query.split('&');
  for (let i = 0; i < vars.length; i++) {
    const pair = vars[i].split('=');
    const k = decodeURIComponent(pair[0]);
    if (k in params) {
      // Duplicate parameters are not allowed to prevent attackers who can
      // append things to |location| from getting their parameter values to
      // override legitimate ones.
      return Object.create(null);
    } else {
      params[k] = decodeURIComponent(pair[1]);
    }
  }
  return params;
}

/**
 * Creates a new most visited link element.
 * @param {Object} params URL parameters containing styles for the link.
 * @param {string} href The destination for the link.
 * @param {string} title The title for the link.
 * @param {string|undefined} text The text for the link or none.
 * @param {string|undefined} direction The text direction.
 * @return {!Element} A new link element.
 */
function createMostVisitedLink(params, href, title, text, direction) {
  const styles = getMostVisitedStyles(params, !!text);
  const link = document.createElement('a');
  link.style.color = styles.color;
  link.style.fontSize = styles.fontSize + 'px';
  if (styles.fontFamily) {
    link.style.fontFamily = styles.fontFamily;
  }
  if (styles.textAlign) {
    link.style.textAlign = styles.textAlign;
  }
  if (styles.textFadePos) {
    const dir = /^rtl$/i.test(direction) ? 'to left' : 'to right';
    // The fading length in pixels is passed by the caller.
    const mask = 'linear-gradient(' + dir + ', rgba(0,0,0,1), rgba(0,0,0,1) ' +
        styles.textFadePos + 'px, rgba(0,0,0,0))';
    link.style.textOverflow = 'clip';
    link.style.webkitMask = mask;
  }
  if (styles.numTitleLines && styles.numTitleLines > 1) {
    link.classList.add('multiline');
  }

  link.href = href;
  link.title = title;
  link.target = '_top';
  // Include links in the tab order.  The tabIndex is necessary for
  // accessibility.
  link.tabIndex = '0';
  if (text) {
    // Wrap text with span so ellipsis will appear at the end of multiline.
    const spanWrap = document.createElement('span');
    spanWrap.textContent = text;
    link.appendChild(spanWrap);
  }
  link.addEventListener('focus', function() {
    window.parent.postMessage('linkFocused', MV_DOMAIN_ORIGIN);
  });
  link.addEventListener('blur', function() {
    window.parent.postMessage('linkBlurred', MV_DOMAIN_ORIGIN);
  });

  link.addEventListener('keydown', function(event) {
    if (event.keyCode == 46 /* DELETE */ ||
        event.keyCode == 8 /* BACKSPACE */) {
      event.preventDefault();
      window.parent.postMessage(
          'tileBlacklisted,' + params['pos'], MV_DOMAIN_ORIGIN);
    } else if (
        event.keyCode == 13 /* ENTER */ || event.keyCode == 32 /* SPACE */) {
      // Event target is the <a> tag. Send a click event on it, which will
      // trigger the 'click' event registered above.
      event.preventDefault();
      event.target.click();
    }
  });

  return link;
}

/**
 * Returns the color to display string with, depending on whether title is
 * displayed, the current theme, and URL parameters.
 * @param {Object<string>} params URL parameters specifying style.
 * @param {boolean} isTitle if the style is for the Most Visited Title.
 * @return {string} The color to use, in "rgba(#,#,#,#)" format.
 */
function getTextColor(params, isTitle) {
  // 'RRGGBBAA' color format overrides everything.
  if ('c' in params && params.c.match(/^[0-9A-Fa-f]{8}$/)) {
    // Extract the 4 pairs of hex digits, map to number, then form rgba().
    const t = params.c.match(/(..)(..)(..)(..)/).slice(1).map(function(s) {
      return parseInt(s, 16);
    });
    return 'rgba(' + t[0] + ',' + t[1] + ',' + t[2] + ',' + t[3] / 255 + ')';
  }

  // For backward compatibility with server-side NTP, look at themes directly
  // and use param.c for non-title or as fallback.
  const apiHandle = chrome.embeddedSearch.newTabPage;
  const ntpTheme = assert(apiHandle.ntpTheme);
  let c = '#777';
  if (isTitle && ntpTheme && !ntpTheme.usingDefaultTheme) {
    // Read from theme directly
    c = convertArrayToRGBAColor(ntpTheme.textColorRgba) || c;
  } else if ('c' in params) {
    c = convertToHexColor(parseInt(params.c, 16)) || c;
  }
  return c;
}

/**
 * Decodes most visited styles from URL parameters.
 * - c: A hexadecimal number interpreted as a hex color code.
 * - f: font-family.
 * - fs: font-size as a number in pixels.
 * - ta: text-align property, as a string.
 * - tf: text fade starting position, in pixels.
 * - ntl: number of lines in the title.
 * @param {Object<string>} params URL parameters specifying style.
 * @param {boolean} isTitle if the style is for the Most Visited Title.
 * @return {Object} Styles suitable for CSS interpolation.
 */
function getMostVisitedStyles(params, isTitle) {
  const styles = {
    color: getTextColor(params, isTitle),  // Handles 'c' in params.
    fontFamily: '',
    fontSize: 11
  };
  if ('f' in params && /^[-0-9a-zA-Z ,]+$/.test(params.f)) {
    styles.fontFamily = params.f;
  }
  if ('fs' in params && isFinite(parseInt(params.fs, 10))) {
    styles.fontSize = parseInt(params.fs, 10);
  }
  if ('ta' in params && /^[-0-9a-zA-Z ,]+$/.test(params.ta)) {
    styles.textAlign = params.ta;
  }
  if ('tf' in params) {
    const tf = parseInt(params.tf, 10);
    if (isFinite(tf)) {
      styles.textFadePos = tf;
    }
  }
  if ('ntl' in params) {
    const ntl = parseInt(params.ntl, 10);
    if (isFinite(ntl)) {
      styles.numTitleLines = ntl;
    }
  }
  return styles;
}

/**
 * Returns whether the given URL has a known, safe scheme.
 * @param {string} url URL to check.
 */
const isSchemeAllowed = function(url) {
  return url.startsWith('http://') || url.startsWith('https://') ||
      url.startsWith('ftp://') || url.startsWith('chrome-extension://');
};

/**
 * @param {!Location} location A location containing URL parameters.
 * @param {function(Object, Object)} fill A function called with styles and
 *     data to fill.
 */
function fillMostVisited(location, fill) {
  const params = parseQueryParams(location);
  params.rid = parseInt(params.rid, 10);
  if (!isFinite(params.rid)) {
    return;
  }
  const data =
      chrome.embeddedSearch.newTabPage.getMostVisitedItemData(params.rid);
  if (!data) {
    return;
  }
  if (data.url && !isSchemeAllowed(data.url)) {
    return;
  }

  if (isFinite(params.dummy) && parseInt(params.dummy, 10)) {
    data.dummy = true;
  }

  if (data.direction) {
    document.body.dir = data.direction;
  }
  fill(params, data);
}
