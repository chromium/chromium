// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

interface Result {}
interface BasicInfo {}
type UserSettings = Record<string, any>;

function initialize() {
  function submitURL(event: Event) {
    getRequiredElement('try-url-result').textContent = '';
    getRequiredElement('manual-allowlist').textContent = '';
    sendWithPromise(
        'tryURL', getRequiredElement<HTMLInputElement>('try-url-input').value)
        .then(({allowResult, manual}) => {
          getRequiredElement('try-url-result').textContent = allowResult;
          getRequiredElement('manual-allowlist').textContent = manual;
        });
    event.preventDefault();
  }

  getRequiredElement('try-url').addEventListener('submit', submitURL);

  // Make the prototype jscontent element disappear.
  jstProcess({}, getRequiredElement('filtering-results-container'));

  addWebUiListener('basic-info-received', receiveBasicInfo);
  addWebUiListener('user-settings-received', receiveUserSettings);
  addWebUiListener('filtering-result-received', receiveFilteringResult);

  chrome.send('registerForEvents');

  chrome.send('getBasicInfo');
}

function highlightIfChanged(
    node: HTMLElement, oldVal: string, newVal: boolean|string) {
  function clearHighlight() {
    node.removeAttribute('highlighted');
  }

  const oldStr = oldVal.toString();
  const newStr = newVal.toString();
  if (oldStr !== '' && oldStr !== newStr) {
    node.onanimationend = clearHighlight;
    node.setAttribute('highlighted', '');
  }
}

function receiveBasicInfo(info: BasicInfo) {
  jstProcess(new JsEvalContext(info), getRequiredElement('basic-info'));

  // Hack: Schedule another refresh after a while.
  setTimeout(function() {
    chrome.send('getBasicInfo');
  }, 5000);
}

function receiveUserSettings(settings: UserSettings|null) {
  if (settings === null) {
    getRequiredElement('user-settings').classList.add('hidden');
    return;
  }

  getRequiredElement('user-settings').classList.remove('hidden');

  // The user settings are returned as an object, flatten them into a
  // list of key/value pairs for easier consumption by the HTML template.
  // This is not done recursively, values are passed as their JSON
  // representation.
  const kvpairs = Object.keys(settings).map(function(key) {
    return {key, value: JSON.stringify(settings[key], null, 2)};
  });

  jstProcess(
      new JsEvalContext({settings: kvpairs}),
      getRequiredElement('user-settings'));
}

/**
 * Helper to determine if an element is scrolled to its bottom limit.
 */
function isScrolledToBottom(elem: HTMLElement): boolean {
  return elem.scrollHeight - elem.scrollTop === elem.clientHeight;
}

/**
 * Helper to scroll an element to its bottom limit.
 */
function scrollToBottom(elem: HTMLElement) {
  elem.scrollTop = elem.scrollHeight - elem.clientHeight;
}

/** Container for accumulated filtering results. */
const filteringResults: Result[] = [];

/**
 * Callback for incoming filtering results.
 */
function receiveFilteringResult(result: Result) {
  filteringResults.push(result);

  const container = getRequiredElement('filtering-results-container');

  // Scroll to the bottom if we were already at the bottom.  Otherwise, leave
  // the scrollbar alone.
  const shouldScrollDown = isScrolledToBottom(container);

  jstProcess(new JsEvalContext({results: filteringResults}), container);

  if (shouldScrollDown) {
    scrollToBottom(container);
  }
}

// Export on window since it is called with jseval.
Object.assign(window, {highlightIfChanged});

document.addEventListener('DOMContentLoaded', initialize);
