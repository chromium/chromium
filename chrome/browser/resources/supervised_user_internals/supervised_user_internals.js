// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.supervised_user_internals', function() {
  'use strict';

  function initialize() {
    function submitURL(event) {
      $('try-url-result').textContent = '';
      $('manual-whitelist').textContent = '';
      $('whitelists').textContent = '';
      chrome.send('tryURL', [$('try-url-input').value]);
      event.preventDefault();
    }

    $('try-url').addEventListener('submit', submitURL);

    // Make the prototype jscontent element disappear.
    jstProcess({}, $('filtering-results-container'));

    chrome.send('registerForEvents');

    chrome.send('getBasicInfo');
  }

  function highlightIfChanged(node, oldVal, newVal) {
    function clearHighlight() {
      this.removeAttribute('highlighted');
    }

    const oldStr = oldVal.toString();
    const newStr = newVal.toString();
    if (oldStr != '' && oldStr != newStr) {
      // Note the addListener function does not end up creating duplicate
      // listeners.  There can be only one listener per event at a time.
      // See https://developer.mozilla.org/en/DOM/element.addEventListener
      node.addEventListener('animationend', clearHighlight, false);
      node.setAttribute('highlighted', '');
    }
  }

  function receiveBasicInfo(info) {
    jstProcess(new JsEvalContext(info), $('basic-info'));

    // Hack: Schedule another refresh after a while.
    setTimeout(function() {
      chrome.send('getBasicInfo');
    }, 5000);
  }

  function receiveUserSettings(settings) {
    if (settings === null) {
      $('user-settings').classList.add('hidden');
      return;
    }

    $('user-settings').classList.remove('hidden');

    // The user settings are returned as an object, flatten them into a
    // list of key/value pairs for easier consumption by the HTML template.
    // This is not done recursively, values are passed as their JSON
    // representation.
    const kvpairs = Object.keys(settings).map(function(key) {
      return {key: key, value: JSON.stringify(settings[key], null, 2)};
    });

    jstProcess(new JsEvalContext({settings: kvpairs}), $('user-settings'));
  }

  function receiveTryURLResult(result) {
    $('try-url-result').textContent = result['allowResult'];
    $('manual-whitelist').textContent = result['manual'];
    $('whitelists').textContent = result['whitelists'];
  }

  /**
   * Helper to determine if an element is scrolled to its bottom limit.
   * @param {Element} elem element to check
   * @return {boolean} true if the element is scrolled to the bottom
   */
  function isScrolledToBottom(elem) {
    return elem.scrollHeight - elem.scrollTop == elem.clientHeight;
  }

  /**
   * Helper to scroll an element to its bottom limit.
   * @param {Element} elem element to be scrolled
   */
  function scrollToBottom(elem) {
    elem.scrollTop = elem.scrollHeight - elem.clientHeight;
  }

  /** Container for accumulated filtering results. */
  const filteringResults = [];

  /**
   * Callback for incoming filtering results.
   * @param {Object} result The result.
   */
  function receiveFilteringResult(result) {
    filteringResults.push(result);

    const container = $('filtering-results-container');

    // Scroll to the bottom if we were already at the bottom.  Otherwise, leave
    // the scrollbar alone.
    const shouldScrollDown = isScrolledToBottom(container);

    jstProcess(new JsEvalContext({results: filteringResults}), container);

    if (shouldScrollDown) {
      scrollToBottom(container);
    }
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    highlightIfChanged: highlightIfChanged,
    receiveBasicInfo: receiveBasicInfo,
    receiveUserSettings: receiveUserSettings,
    receiveTryURLResult: receiveTryURLResult,
    receiveFilteringResult: receiveFilteringResult,
  };
});

document.addEventListener(
    'DOMContentLoaded', chrome.supervised_user_internals.initialize);
