// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

type RuleSet = {
  sitelist: Array<string>;
  greylist: Array<string>;
};

type RuleSetList = {
  gpo: RuleSet;
  ieem?: RuleSet;
  external?: RuleSet;
};

/**
 * Returned by getRulesetSources().
 */
type RulesetSources = {
  browser_switcher: {[k: string]: string};
};

/**
 * Returned by getTimestamps().
 */
type TimestampPair = {
  last_fetch: number;
  next_fetch: number;
};

/**
 * Converts 'this_word' to 'ThisWord'
 */
function snakeCaseToUpperCamelCase(symbol: string): string {
  if (!symbol) {
    return symbol;
  }
  return symbol.replace(/(?:^|_)([a-z])/g, (_, letter) => {
    return letter.toUpperCase();
  });
}

/**
 * Clears the table, and inserts a header row.
 * @param headerTemplate Template to use to re-create the header row.
 */
function clearTable(
    table: HTMLTableElement, headerTemplate: HTMLTemplateElement) {
  table.innerHTML = '';
  const headerRow = document.importNode(headerTemplate.content, true);
  table.appendChild(headerRow);
}

/**
 * @return String describing the rule type.
 */
function getRuleType(rule: string): string {
  if (rule == '*') {
    return 'wildcard';
  }
  if (rule.includes('/')) {
    return 'prefix';
  }
  return 'hostname';
}

/**
 * Creates and returns a <tr> element for the given rule.
 */
function createRowForRule(
    rule: string, rulesetName: string): HTMLTableRowElement {
  const row = document.importNode(
                  ($('rule-row-template') as HTMLTemplateElement).content,
                  true) as unknown as HTMLTableRowElement;
  const cells = row.querySelectorAll('td');
  cells[0].innerText = rule;
  cells[0].className = 'url';
  cells[1].innerText = rulesetName;
  cells[2].innerText = getRuleType(rule);
  cells[3].innerText = /^!/.test(rule) ? 'yes' : 'no';
  return row;
}

/**
 * Updates the content of all tables after receiving data from the backend.
 */
function updateTables(rulesets: RuleSetList) {
  const headerTemplate = $('header-row-template') as HTMLTemplateElement;
  clearTable($('sitelist') as HTMLTableElement, headerTemplate);
  clearTable($('greylist') as HTMLTableElement, headerTemplate);

  for (const [rulesetName, ruleset] of Object.entries(rulesets)) {
    for (const [listName, rules] of Object.entries(ruleset as RuleSet)) {
      const table = $(listName);
      for (const rule of rules) {
        table.appendChild(createRowForRule(rule, rulesetName));
      }
    }
  }
}

function checkUrl() {
  const url = ($('url-checker-input') as HTMLInputElement).value;
  if (!url) {
    $('output').innerText = '';
    return;
  }
  sendWithPromise('getDecision', url)
      .then(decision => {
        // URL is valid.
        $('output').innerText = JSON.stringify(decision, null, 2);
      })
      .catch(() => {
        // URL is invalid.
        $('output').innerText =
            'Invalid URL. Make sure it is formatted properly.';
      });
}

$('url-checker-input').addEventListener('input', checkUrl);

/**
 * Formats |date| as "HH:MM:SS".
 */
function formatTime(date: Date): string {
  const hh = date.getHours().toString().padStart(2, '0');
  const mm = date.getMinutes().toString().padStart(2, '0');
  const ss = date.getSeconds().toString().padStart(2, '0');
  return `${hh}:${mm}:${ss}`;
}

/**
 * Update the paragraphs under the "XML sitelists" section.
 */
function updateTimestamps(timestamps: TimestampPair|null) {
  if (!timestamps) {
    return;
  }

  const lastFetch = new Date(timestamps.last_fetch);
  const nextFetch = new Date(timestamps.next_fetch);

  if (lastFetch.valueOf() == 0) {
    // Not fetched yet.
    $('xml-not-fetched-yet').style.display = 'block';
    $('xml-last-fetch').style.display = 'none';
  } else {
    // Already fetched.
    $('xml-not-fetched-yet').style.display = 'none';
    $('xml-last-fetch').style.display = 'block';
  }

  $('xml-next-fetch').style.display = nextFetch.valueOf() ? 'block' : 'none';

  $('last-fetch-placeholder').innerText = formatTime(lastFetch);
  $('next-fetch-placeholder').innerText = formatTime(nextFetch);
}

/**
 * Update the table under the "XML sitelists" section.
 */
function updateXmlTable({browser_switcher: sources}: RulesetSources) {
  const headerTemplate = $('xml-header-row-template') as HTMLTemplateElement;
  clearTable($('xml-sitelists') as HTMLTableElement, headerTemplate);

  for (const [prefName, url] of Object.entries(sources)) {
    // Hacky: guess the policy name from the pref name by converting 'foo_bar'
    // to 'BrowserSwitcherFooBar'. This relies on prefs having the same name as
    // the associated policy.
    const policyName = 'BrowserSwitcher' + snakeCaseToUpperCamelCase(prefName);
    const row = document.importNode(
        ($('xml-row-template') as HTMLTemplateElement).content, true);
    const cells = row.querySelectorAll('td');
    cells[0].innerText = policyName;
    cells[1].innerText = url || '(not configured)';
    cells[1].className = 'url';
    $('xml-sitelists').appendChild(row);
  }

  // Hide/show the description paragraphs depending on whether any XML sitelist
  // is configured.
  const enabled = Object.values(sources).some(x => !!x);
  $('xml-description-wrapper').style.display = enabled ? 'block' : 'none';
}

/**
 * Called by C++ when we need to update everything on the page.
 */
function updateEverything() {
  sendWithPromise('getAllRulesets').then(updateTables);
  sendWithPromise('getTimestamps').then(updateTimestamps);
  sendWithPromise('getRulesetSources').then(updateXmlTable);
  checkUrl();
}

// TODO(crbug/959379): Keep the page up-to-date at all times: updateEverything()
// when LBS prefs are updated, and after sitelists get refreshed.

updateEverything();

$('refresh-xml-button').addEventListener('click', () => {
  chrome.send('refreshXml');
});
