// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, render} from '//resources/lit/v3_0/lit.rollup.js';
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

interface Result {
  reason: string;
  result: string;
  url: string;
}

interface DataEntry {
  is_valid: boolean;
  stat_name: string;
  stat_value: string|boolean;
}

interface Section {
  title: string;
  data: DataEntry[];
}

interface BasicInfo {
  sections: Section[];
}

type UserSettings = Record<string, any>;

function getBasicInfoHtml(sections: Section[]) {
  // clang-format off
  return html`
    ${sections.map((section, sectionIndex) => html`
      <div class="section">
        <h2>${section.title}</h2>
        <table class="section-details">
          ${section.data.map((item, dataIndex) => html`
            <tr class="${item.is_valid ? '' : 'uninitialized'}"
                ?highlighted="${
                      shouldHighlight(item, sectionIndex, dataIndex)}">
              <td class="detail" width=50%>${item.stat_name}</td>
              <td class="value" width=50%>${item.stat_value}</td>
            </tr>
          `)}
        </table>
      </div>
    `)}
  `;
  // clang-format on
}

function getUserSettingsHtml(pairs: Array<{key: string, value: string}>) {
  if (pairs.length === 0) {
    return html``;
  }

  // clang-format off
  return html`
    <h2>Family Link User Settings</h2>
    <table class="section-details">
      ${pairs.map(pair => html`
        <tr>
          <td>${pair.key}</td>
          <td><pre>${pair.value}</pre></td>
        </tr>
      `)}
    </table>`;
  // clang-format on
}

function getFilteringResultsHtml(results: Result[]) {
  // clang-format off
  return html`
    ${results.map(item => html`
      <div class="filtering-results-entry">
        <div class="url">${item.url}</div>
        <span class="result">${item.result}</span>
        <span class="reason">${item.reason}</span>
      </div>
    `)}
  `;
  // clang-format on
}

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

  addWebUiListener('basic-info-received', receiveBasicInfo);
  addWebUiListener('user-settings-received', receiveUserSettings);
  addWebUiListener('filtering-result-received', receiveFilteringResult);

  chrome.send('registerForEvents');
  chrome.send('getBasicInfo');
}

let previousSections: Section[] = [];

function shouldHighlight(
    item: DataEntry, sectionIndex: number, dataIndex: number): boolean {
  if (sectionIndex >= previousSections.length) {
    return false;
  }

  if (dataIndex >= previousSections[sectionIndex]!.data.length) {
    return false;
  }

  return previousSections[sectionIndex]!.data[dataIndex]!.stat_value !==
      item.stat_value;
}

function receiveBasicInfo(info: BasicInfo) {
  render(getBasicInfoHtml(info.sections), getRequiredElement('basic-info'));
  previousSections = info.sections;

  // Hack: Schedule another refresh after a while.
  setTimeout(function() {
    chrome.send('getBasicInfo');
  }, 5000);
}

function receiveUserSettings(settings: UserSettings) {
  // The user settings are returned as an object, flatten them into a
  // list of key/value pairs for easier consumption by the HTML template.
  // This is not done recursively, values are passed as their JSON
  // representation.
  const kvpairs = Object.keys(settings).map(function(key) {
    return {key, value: JSON.stringify(settings[key], null, 2)};
  });

  render(getUserSettingsHtml(kvpairs), getRequiredElement('user-settings'));
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

  render(getFilteringResultsHtml(filteringResults), container);

  if (shouldScrollDown) {
    scrollToBottom(container);
  }
}

document.addEventListener('DOMContentLoaded', initialize);
