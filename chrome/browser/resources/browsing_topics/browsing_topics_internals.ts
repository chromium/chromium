// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assert} from 'chrome://resources/js/assert.js';
import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {Time, TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import type {PageHandlerRemote, WebUITopic} from './browsing_topics_internals.mojom-webui.js';
import {PageHandler} from './browsing_topics_internals.mojom-webui.js';

let pageHandler: PageHandlerRemote|null = null;
let hostsClassificationSequenceNumber = 0;

function setElementVisible(id: string, visible: boolean) {
  const element = document.querySelector<HTMLDivElement>('#' + id);
  element!.style.display = visible ? 'block' : 'none';
}

function setButtonEnabled(id: string, enabled: boolean) {
  const element = document.querySelector<HTMLButtonElement>('#' + id);
  element!.disabled = !enabled;
}

function decodeString16(arr: String16) {
  return arr.data.map(ch => String.fromCodePoint(ch)).join('');
}

function formatTimeDuration(totalMicrosecondsBigInt: bigint) {
  if (totalMicrosecondsBigInt > Number.MAX_SAFE_INTEGER) {
    return '+inf';
  }

  const totalMicroseconds = Number(totalMicrosecondsBigInt);

  let totalSeconds = Math.floor(totalMicroseconds / 1000000);

  const days = Math.floor(totalSeconds / 3600 / 24);
  totalSeconds %= 3600 * 24;
  const hours = Math.floor(totalSeconds / 3600);
  totalSeconds %= 3600;
  const minutes = Math.floor(totalSeconds / 60);
  const seconds = Math.floor(totalSeconds % 60);
  return days + 'd-' + hours + 'h-' + minutes + 'm-' + seconds + 's';
}

function formatMojoTime(mojoTime: Time) {
  // The JS Date() is based off of the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // base::Time (represented in mojom.Time) represents the number of
  // microseconds since the Windows FILETIME epoch (1601-01-01 00:00:00 UTC).
  // This computes the final JS time by computing the epoch delta and the
  // conversion from microseconds to milliseconds.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return (new Date(timeInMs - epochDeltaInMs)).toLocaleString();
}

function getEnabledStatusText(enabled: boolean) {
  return enabled ? 'enabled' : 'disabled';
}

function getRealOrRandomStatusText(isRealTopic: boolean) {
  return isRealTopic ? 'Real' : 'Random';
}

function createContextDomainEntry(domain: string) {
  const entry =
      document
          .querySelector<HTMLTemplateElement>(
              '#context-domain-entry-template')!.content.cloneNode(true);
  (entry as HTMLElement).querySelectorAll('span')[0]!.textContent = domain;
  return entry;
}

function createTopicRow(topic: WebUITopic) {
  const row =
      document.querySelector<HTMLTemplateElement>(
                  '#topic-row-template')!.content.cloneNode(true) as
      HTMLElement;
  const nestedCells = row.querySelectorAll('td');
  nestedCells[0]!.textContent = String(topic.topicId);
  nestedCells[1]!.textContent = decodeString16(topic.topicName);
  nestedCells[2]!.textContent = getRealOrRandomStatusText(topic.isRealTopic);

  topic.observedByDomains.forEach((domain) => {
    row.querySelectorAll('td')[3]!.appendChild(
        createContextDomainEntry(domain));
  });

  return row;
}

function fieldNameFromId(id: string) {
  const tokens = id.split('-');
  let processedFirstToken = false;

  const convertedTokens = tokens.map(token => {
    if (!processedFirstToken) {
      processedFirstToken = true;
      return token;
    }

    if (token === 'div') {
      return '';
    }

    // Capitalize the first letter
    return token.charAt(0).toUpperCase() + token.slice(1);
  });

  return convertedTokens.join('');
}

async function asyncGetBrowsingTopicsConfiguration() {
  assert(pageHandler);
  const response = await pageHandler.getBrowsingTopicsConfiguration();

  const config = response.config;

  // Enabled status fields
  ['browsing-topics-enabled-div',
   'privacy-sandbox-ads-apis-override-enabled-div',
   'override-privacy-sandbox-settings-local-testing-enabled-div',
   'browsing-topics-bypass-ip-is-publicly-routable-check-enabled-div',
   'browsing-topics-document-api-enabled-div',
   'browsing-topics-parameters-enabled-div']
      .forEach(id => {
        const div = document.querySelector<HTMLElement>(`#${id}`);
        assert(div);
        div.textContent! += getEnabledStatusText(
            config[fieldNameFromId(id) as keyof typeof config] as boolean);
      });

  // Number fields
  ['config-version-div', 'number-of-epochs-to-expose-div',
   'number-of-top-topics-per-epoch-div',
   'use-random-topic-probability-percent-div',
   'number-of-epochs-of-observation-data-to-use-for-filtering-div',
   'max-number-of-api-usage-context-domains-to-keep-per-topic-div',
   'max-number-of-api-usage-context-entries-to-load-per-epoch-div',
   'max-number-of-api-usage-context-domains-to-store-per-page-load-div',
   'taxonomy-version-div', 'disabled-topics-list-div']
      .forEach(id => {
        const div = document.querySelector<HTMLElement>(`#${id}`);
        assert(div);
        div.textContent! +=
            (config[fieldNameFromId(id) as keyof typeof config] as number);
      });

  // Time duration fields
  ['time-period-per-epoch-div', 'max-epoch-introduction-delay-div'].forEach(
      id => {
        const div = document.querySelector<HTMLElement>(`#${id}`);
        assert(div);
        div.textContent! += formatTimeDuration(
            (config[fieldNameFromId(id) as keyof typeof config] as TimeDelta)
                .microseconds);
      });
}

async function asyncGetBrowsingTopicsState(calculateNow: boolean) {
  // Clear and hide existing content.
  document.querySelector('#epoch-div-list-wrapper')!.innerHTML =
      window.trustedTypes!.emptyHTML;
  setElementVisible('topics-state-override-status-message-div', false);
  setElementVisible('topics-state-div', false);

  // Disable the buttons to make sure only one request (either Refresh or
  // Calculate Now) can be made at a time.
  setButtonEnabled('refresh-topics-state-button', false);
  setButtonEnabled('calculate-now-button', false);

  assert(pageHandler);
  const response = await pageHandler.getBrowsingTopicsState(calculateNow);

  setButtonEnabled('refresh-topics-state-button', true);
  setButtonEnabled('calculate-now-button', true);

  const result = response.result;

  if (result.overrideStatusMessage) {
    document.querySelector(
                '#topics-state-override-status-message-div')!.textContent =
        result.overrideStatusMessage.toString();
    setElementVisible('topics-state-override-status-message-div', true);
    return;
  }

  setElementVisible('topics-state-div', true);

  document.querySelector('#next-scheduled-calculation-time-div')!.textContent =
      'Next scheduled calculation time: ' +
      formatMojoTime(result.browsingTopicsState!.nextScheduledCalculationTime);

  result.browsingTopicsState!.epochs.forEach((epoch) => {
    const epochDiv =
        document.querySelector<HTMLTemplateElement>(
                    '#epoch-div-template')!.content.cloneNode(true) as
        HTMLElement;

    const nestedDivs = epochDiv.querySelectorAll('div');
    nestedDivs[1]!.textContent += formatMojoTime(epoch.calculationTime);
    nestedDivs[2]!.textContent += epoch.modelVersion;
    nestedDivs[3]!.textContent += epoch.taxonomyVersion;

    epoch.topics.forEach((topic) => {
      epochDiv.querySelectorAll('table')[0]!.appendChild(createTopicRow(topic));
    });

    document.querySelector('#epoch-div-list-wrapper')!.appendChild(epochDiv);
  });
}

function createClassificationResultTopicEntry(topic: string) {
  const entry = document
                    .querySelector<HTMLTemplateElement>(
                        '#classification-result-topic-entry-template')!.content
                    .cloneNode(true);
  (entry as HTMLElement).querySelectorAll('span')[0]!.textContent = topic;
  return entry;
}

function createClassificationResultRow(host: string, topics: WebUITopic[]) {
  const row = document
                  .querySelector<HTMLTemplateElement>(
                      '#classification-result-host-row-template')!.content
                  .cloneNode(true) as HTMLElement;
  const nestedCells = row.querySelectorAll('td');
  nestedCells[0]!.textContent = host;

  topics.forEach((topic) => {
    const topicText =
        String(topic.topicId) + '. ' + decodeString16(topic.topicName);
    nestedCells[1]!.appendChild(
        createClassificationResultTopicEntry(topicText));
  });

  return row;
}

async function asyncClassifyHosts(hosts: string[], sequenceNumber: number) {
  let topicsForHosts = [] as WebUITopic[][];

  if (hosts.length > 0) {
    assert(pageHandler);
    const response = await pageHandler.classifyHosts(hosts);
    topicsForHosts = response.topicsForHosts;
  }

  // Skip this result if a newer classification was initiated before this one
  // finished.
  if (sequenceNumber !== hostsClassificationSequenceNumber) {
    return;
  }

  for (let i = 0; i < hosts.length; i++) {
    const host = hosts[i] as string;
    const topics = topicsForHosts![i] as WebUITopic[];

    document.querySelector('#hosts-classification-result-table')!.appendChild(
        createClassificationResultRow(host, topics));
  }

  setElementVisible('hosts-classification-loader-div', false);
  setElementVisible('hosts-classification-result-table-wrapper', true);
}

function clearHostsClassificationResult() {
  const table = document.querySelector<HTMLTableElement>(
      '#hosts-classification-result-table');
  assert(table);

  while (table.rows[1]) {
    table.deleteRow(1);
  }

  const div = document.querySelector<HTMLElement>(
      '#hosts-classification-input-validation-error');
  div!.innerHTML = window.trustedTypes!.emptyHTML;

  setElementVisible('hosts-classification-loader-div', false);
  setElementVisible('hosts-classification-input-validation-error', false);
  setElementVisible('hosts-classification-result-table-wrapper', false);
}

async function asyncGetModelInfo() {
  assert(pageHandler);
  const response = await pageHandler.getModelInfo();

  setElementVisible('model-info-loader', false);

  const result = response.result;

  if (result.overrideStatusMessage) {
    document.querySelector(
                '#model-info-override-status-message-div')!.textContent =
        result.overrideStatusMessage.toString();
    setElementVisible('model-info-override-status-message-div', true);
    return;
  }

  document.querySelector('#model-version-div')!.textContent +=
      result.modelInfo!.modelVersion;
  document.querySelector('#model-file-path-div')!.textContent +=
      result.modelInfo!.modelFilePath;

  setElementVisible('model-info-div', true);
  setElementVisible('hosts-classification-input-area-div', true);

  document.querySelector(
              '#hosts-classification-button')!.addEventListener('click', () => {
    clearHostsClassificationResult();

    const input =
        document.querySelector<HTMLTextAreaElement>(
                    '#input-hosts-textarea')!.value;
    const hosts = input!.split('\n');

    const preprocessedHosts = [] as string[];
    hosts.forEach((host) => {
      const trimmedHost = host.trim();
      if (trimmedHost === '') {
        return;
      }

      preprocessedHosts.push(trimmedHost);
    });

    const inputValidationErrors = [] as string[];
    preprocessedHosts.forEach((host) => {
      if (host.includes('/')) {
        inputValidationErrors.push(
            'Host \"' + host + '" contains invalid character: "\/"');
      }
    });

    ++hostsClassificationSequenceNumber;

    if (inputValidationErrors.length > 0) {
      const errorsDiv = document.querySelector<HTMLElement>(
          '#hosts-classification-input-validation-error');
      inputValidationErrors.forEach((error) => {
        const errorDiv = document.createElement('div');
        errorDiv.textContent = error;
        errorsDiv!.appendChild(errorDiv);
      });

      setElementVisible('hosts-classification-input-validation-error', true);
    } else {
      setElementVisible('hosts-classification-loader-div', true);
      asyncClassifyHosts(preprocessedHosts, hostsClassificationSequenceNumber);
    }
  });
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup the mojo interface.
  pageHandler = PageHandler.getRemote();

  setElementVisible('topics-state-override-status-message-div', false);
  setElementVisible('topics-state-div', false);
  setElementVisible('model-info-override-status-message-div', false);
  setElementVisible('model-info-div', false);
  setElementVisible('hosts-classification-input-area-div', false);
  setElementVisible('hosts-classification-loader-div', false);
  setElementVisible('hosts-classification-input-validation-error', false);
  setElementVisible('hosts-classification-result-table-wrapper', false);

  asyncGetBrowsingTopicsConfiguration();
  asyncGetModelInfo();

  document.querySelector('#refresh-topics-state-button')!.addEventListener(
      'click', () => {
        asyncGetBrowsingTopicsState(/*calculateNow=*/ false);
      });

  document.querySelector('#calculate-now-button')!.addEventListener(
      'click', () => {
        asyncGetBrowsingTopicsState(/*calculateNow=*/ true);
      });

  asyncGetBrowsingTopicsState(/*calculateNow=*/ false);
});
