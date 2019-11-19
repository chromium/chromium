// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Code to execute before Closure's base.js.
 *
 */

/**
 * Tell Closure to load JavaScript code from the extension root directory.
 * @type {boolean}
 */
window.CLOSURE_BASE_PATH = chrome.extension.getURL('/closure/');

/**
 * Tell Closure not to load deps.js; it's included by manifest.json already.
 * @type {boolean}
 */
window.CLOSURE_NO_DEPS = true;

/**
 * Array of urls that should be included next, in order.
 * @type {Array}
 * @private
 */
window.queue_ = [];

/**
 * Custom function for importing ChromeVox scripts.
 * @param {string} src The JS file to import.
 * @return {boolean} Whether the script was imported.
 */
window.CLOSURE_IMPORT_SCRIPT = function(src) {
  // Only run our version of the import script
  // when trying to inject ChromeVox scripts.
  // TODO(lazyboy): Use URL instead.
  if (src.startsWith('chrome-extension://')) {
    if (!goog.inHtmlDocument_() || goog.dependencies_.written[src]) {
      return false;
    }
    goog.dependencies_.written[src] = true;
    function loadNextScript() {
      if (goog.global.queue_.length == 0) {
        return;
      }

      var src = goog.global.queue_[0];

      if (window.CLOSURE_USE_EXT_MESSAGES) {
        var relativeSrc = src.substr(src.indexOf('closure/..') + 11);
        chrome.extension.sendMessage(
            {'srcFile': relativeSrc}, function(response) {
              try {
                eval(response['code']);
              } catch (e) {
                console.error('Script error: ' + e + ' in ' + src);
              }
              goog.global.queue_ = goog.global.queue_.slice(1);
              loadNextScript();
            });
        return;
      }
      window.console.log('Using XHR');

      // Load the script by fetching its source and running 'eval' on it
      // directly, with a magic comment that makes Chrome treat it like it
      // loaded normally. Wait until it's fetched before loading the
      // next script.
      var xhr = new XMLHttpRequest();
      var url = src + '?' + new Date().getTime();
      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4) {
          var scriptText = xhr.responseText;
          // Add a magic comment to the bottom of the file so that
          // Chrome knows the name of the script in the JavaScript debugger.
          scriptText += '\n//# sourceURL=' + src + '\n';
          eval(scriptText);
          goog.global.queue_ = goog.global.queue_.slice(1);
          loadNextScript();
        }
      };
      xhr.open('GET', url, false);
      xhr.send(null);
    }
    goog.global.queue_.push(src);
    if (goog.global.queue_.length == 1) {
      loadNextScript();
    }
    return true;
  } else {
    return goog.writeScriptTag_(src);
  }
};
