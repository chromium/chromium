// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Time, TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {PageHandler, WebUITopic} from './browsing_topics_internals.mojom-webui.js';

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
  const response =
      await PageHandler.getRemote().getBrowsingTopicsConfiguration();

  const config = response.config;

  // Enabled status fields
  ['browsing-topics-enabled-div',
   'privacy-sandbox-ads-apis-override-enabled-div',
   'privacy-sandbox-settings3-enabled-div',
   'override-privacy-sandbox-settings-local-testing-enabled-div',
   'browsing-topics-bypass-ip-is-publicly-routable-check-enabled-div']
      .forEach(id => {
        const div = document.querySelector<HTMLElement>(`#${id}`);
        assert(div);
        div.textContent! += getEnabledStatusText(
            config[fieldNameFromId(id) as keyof typeof config] as boolean);
      });

  // Number fields
  ['number-of-epochs-to-expose-div', 'number-of-top-topics-per-epoch-div',
   'use-random-topic-probability-percent-div',
   'number-of-epochs-of-observation-data-to-use-for-filtering-div',
   'max-number-of-api-usage-context-domains-to-keep-per-topic-div',
   'max-number-of-api-usage-context-entries-to-load-per-epoch-div',
   'max-number-of-api-usage-context-domains-to-store-per-page-load-div',
   'config-version-div', 'taxonomy-version-div']
      .forEach(id => {
        const div = document.querySelector<HTMLElement>(`#${id}`);
        assert(div);
        div.textContent! +=
            (config[fieldNameFromId(id) as keyof typeof config] as number);
      });

  // Time duration fields
  ['time-period-per-epoch-div'].forEach(id => {
    const div = document.querySelector<HTMLElement>(`#${id}`);
    assert(div);
    div.textContent! += formatTimeDuration(
        (config[fieldNameFromId(id) as keyof typeof config] as TimeDelta)
            .microseconds);
  });
}

async function asyncGetBrowsingTopicsState() {
  const response = await PageHandler.getRemote().getBrowsingTopicsState();

  const result = response.result;

  if (result.overrideStatusMessage) {
    document.querySelector(
                '#topics-state-override-status-message-div')!.textContent =
        result.overrideStatusMessage.toString();
    document.querySelector<HTMLDivElement>('#topics-state-div')!.style.display =
        'none';
    return;
  }

  document.querySelector('#next-scheduled-calculation-time-div')!.textContent +=
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
      epochDiv.querySelectorAll('table')![0]!.appendChild(
          createTopicRow(topic));
    });

    document.querySelector('#epoch-div-list-wrapper')!.appendChild(epochDiv);
  });
}

document.addEventListener('DOMContentLoaded', function() {
  asyncGetBrowsingTopicsConfiguration();
  asyncGetBrowsingTopicsState();
});
