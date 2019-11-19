// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @typedef {{
 *   sitelist: Array<string>,
 *   greylist: Array<string>,
 * }}
 */
let RuleSet;

/**
 * @typedef {{
 *   gpo: RuleSet,
 *   ieem: (RuleSet|undefined),
 *   external: (RuleSet|undefined),
 * }}
 */
let RuleSetList;

/**
 * Returned by getRulesetSources().
 * @typedef {{
 *   browser_switcher: Object<string, string>!,
 * }}
 */
let RulesetSources;

/**
 * Returned by getTimestamps().
 * @typedef {{
 *   last_fetch: number,
 *   next_fetch: number,
 * }}
 */
let TimestampPair;

/**
 * Converts 'this_word' to 'ThisWord'
 * @param {string} symbol
 * @return {string}
 */
function snakeCaseToUpperCamelCase(symbol) {
  if (!symbol) {
    return symbol;
  }
  return symbol.replace(/(?:^|_)([a-z])/g, (_, letter) => {
    return letter.toUpperCase();
  });
}

/**
 * Clears the table, and inserts a header row.
 * @param {HTMLTableElement} table
 * @param {HTMLTemplateElement} headerTemplate
 *     Template to use to re-create the header row.
 */
function clearTable(table, headerTemplate) {
  table.innerHTML = '';
  const headerRow = document.importNode(headerTemplate.content, true);
  table.appendChild(headerRow);
}

/**
 * @param {string} rule
 * @return {string} String describing the rule type.
 */
function getRuleType(rule) {
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
 * @param {string} rule
 * @param {string} rulesetName
 * @return {HTMLTableRowElement}
 */
function createRowForRule(rule, rulesetName) {
  const row = document.importNode($('rule-row-template').content, true);
  const cells = row.querySelectorAll('td');
  cells[0].innerText = rule;
  cells[0].className = 'url';
  cells[1].innerText = rulesetName;
  cells[2].innerText = getRuleType(rule);
  cells[3].innerText = /^!/.test(rule) ? 'yes' : 'no';
  return /** @type {HTMLTableRowElement} */ (row);
}

/**
 * Updates the content of all tables after receiving data from the backend.
 * @param {RuleSetList} rulesets
 */
function updateTables(rulesets) {
  const headerTemplate =
      /** @type {HTMLTemplateElement} */ ($('header-row-template'));
  clearTable(/** @type {HTMLTableElement} */ ($('sitelist')), headerTemplate);
  clearTable(/** @type {HTMLTableElement} */ ($('greylist')), headerTemplate);

  for (const [rulesetName, ruleset] of Object.entries(rulesets)) {
    for (const [listName, rules] of Object.entries(ruleset)) {
      const table = $(listName);
      for (const rule of rules) {
        table.appendChild(createRowForRule(rule, rulesetName));
      }
    }
  }
}

function checkUrl() {
  const url = $('url-checker-input').value;
  if (!url) {
    $('output').innerText = '';
    return;
  }
  cr.sendWithPromise('getDecision', url)
      .then(decision => {
        // URL is valid.
        $('output').innerText = JSON.stringify(decision, null, 2);
      })
      .catch(err => {
        // URL is invalid.
        $('output').innerText =
            'Invalid URL. Make sure it is formatted properly.';
      });
}

$('url-checker-input').addEventListener('input', checkUrl);

/**
 * Formats |date| as "HH:MM:SS".
 * @param {Date} date
 * @return {string}
 */
function formatTime(date) {
  const hh = date.getHours().toString().padStart(2, '0');
  const mm = date.getMinutes().toString().padStart(2, '0');
  const ss = date.getSeconds().toString().padStart(2, '0');
  return `${hh}:${mm}:${ss}`;
}

/**
 * Update the paragraphs under the "XML sitelists" section.
 * @param {TimestampPair?} timestamps
 */
function updateTimestamps(timestamps) {
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
 * @param {RulesetSources} sources
 */
function updateXmlTable({browser_switcher: sources}) {
  const headerTemplate =
      /** @type {HTMLTemplateElement} */ ($('xml-header-row-template'));
  clearTable(
      /** @type {HTMLTableElement} */ ($('xml-sitelists')), headerTemplate);

  for (const [prefName, url] of Object.entries(sources)) {
    // Hacky: guess the policy name from the pref name by converting 'foo_bar'
    // to 'BrowserSwitcherFooBar'. This relies on prefs having the same name as
    // the associated policy.
    const policyName = 'BrowserSwitcher' + snakeCaseToUpperCamelCase(prefName);
    const row = document.importNode($('xml-row-template').content, true);
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
  cr.sendWithPromise('getAllRulesets').then(updateTables);
  cr.sendWithPromise('getTimestamps').then(updateTimestamps);
  cr.sendWithPromise('getRulesetSources').then(updateXmlTable);
  checkUrl();
}

// TODO(crbug/959379): Keep the page up-to-date at all times: updateEverything()
// when LBS prefs are updated, and after sitelists get refreshed.

updateEverything();

$('refresh-xml-button').addEventListener('click', () => {
  chrome.send('refreshXml');
});
