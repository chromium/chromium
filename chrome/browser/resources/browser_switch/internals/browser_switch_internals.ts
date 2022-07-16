// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '../strings.m.js';

import {addWebUIListener, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.m.js';

type Decision = {
  action: 'stay' | 'go';
  matching_rule?: string;
  reason: 'globally_disabled' | 'protocol' | 'sitelist' | 'greylist' | 'default';
};

type RuleSet = {
  sitelist: Array<string>;
  greylist: Array<string>;
};

type RuleSetList = {
  gpo: RuleSet;
  ieem?: RuleSet;
  external?: RuleSet;
};

type ListName = 'sitelist'|'greylist';

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
    rule: string, rulesetName: string,
    listType: ListName): HTMLTableRowElement {
  const templateName = `rule-row-template-${listType}`;
  const row = document.importNode(
                  ($(templateName) as HTMLTemplateElement).content, true) as
      unknown as HTMLTableRowElement;
  const cells = row.querySelectorAll('td');
  cells[0].innerText = rule;
  cells[0].className = 'url';
  cells[1].innerText = rulesetName;
  cells[2].innerText = getRuleType(rule);
  if (listType === 'sitelist') {
    cells[3].innerText =
        rule.startsWith('!') ? getBrowserName() : getAltBrowserName();
  }

  return row;
}

/**
 * Updates the content of all tables after receiving data from the backend.
 */
function updateTables(rulesets: RuleSetList) {
  const siteListHeaderTemplate =
      $('header-row-template-sitelist') as HTMLTemplateElement;
  const greyListHeaderTemplate =
      $('header-row-template-greylist') as HTMLTemplateElement;
  clearTable($('sitelist') as HTMLTableElement, siteListHeaderTemplate);
  clearTable($('greylist') as HTMLTableElement, greyListHeaderTemplate);

  for (const [rulesetName, ruleset] of Object.entries(rulesets)) {
    for (const [listName, rules] of Object.entries(ruleset as RuleSet)) {
      const table = $(listName);
      for (const rule of rules) {
        table.appendChild(
            createRowForRule(rule, rulesetName, listName as ListName));
      }
    }
  }
}

/**
 * Gets the English name of the alternate browser.
 */
function getAltBrowserName(): string {
  // TODO (crbug.com/1258133): if you change the AlternativeBrowserPath policy,
  // then loadTimeData can contain stale data. It won't update until you refresh
  // (despite the rest of the page auto-updating).
  return loadTimeData.getString('altBrowserName');
}

/**
 * Gets the English name of the browser.
 */
function getBrowserName(): string {
  // TODO (crbug.com/1258133): if you change the AlternativeBrowserPath policy,
  // then loadTimeData can contain stale data. It won't update until you refresh
  // (despite the rest of the page auto-updating).
  return loadTimeData.getString('browserName');
}

/**
 * Takes the json from the url checker and makes it readable.
 */
function urlOutputText(decision: Decision): Array<string> {
  let opensIn = '';
  const altBrowserName = getAltBrowserName();
  const browserName = getBrowserName();

  switch (decision.action) {
    case 'stay':
      opensIn = `Opens in: ${browserName}\n`;
      break;
    case 'go':
      opensIn = `Opens in: ${altBrowserName}\n`;
      break;
  }

  let reason = '';
  if (decision.matching_rule) {
    if (decision.matching_rule.startsWith('!')) {
      reason += `Reason: The inverted rule ${JSON.stringify(decision.matching_rule)} was found in `;
    } else {
      reason += `Reason: ${JSON.stringify(decision.matching_rule)} was found in `;
    }
  }
  // if undefined - add nothing to the output

  switch (decision.reason) {
    case 'globally_disabled':
      reason += 'Reason: The BrowserSwitcherEnabled policy is false.\n';
      break;
    case 'protocol':
      reason +=
          'Reason: LBS only supports http://, https://, and file:// URLs.\n';
      break;
    case 'sitelist':
      reason += 'the "Force open in" list.\n';
      break;
    case 'greylist':
      reason += 'the "Ignore" list.\n';
      break;
    case 'default':
      reason += `Reason: LBS stays in ${browserName} by default.\n`;
      break;
  }

  return [opensIn, reason];
}

function checkUrl() {
  let url = ($('url-checker-input') as HTMLInputElement).value;
  if (!url) {
    $('output').innerText = '';
    return;
  }
  if (!url.includes('://')) {
    url = 'http://' + url;
  }
  sendWithPromise('getDecision', url)
      .then((decision) => {
        // URL is valid.
        const [output, reason] = urlOutputText(decision);
        $('output').innerText = output;
        $('reason').innerText = reason;
      })
      .catch((errorMessage) => {
        // URL is invalid.
        console.warn(errorMessage);
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

function generateStaticContent() {
  $('greylist-description').innerText =
      `URLs matching these rules won't trigger a browser switch and can be open in either ${
          getBrowserName()} or ${getAltBrowserName()}.`;
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

document.addEventListener('DOMContentLoaded', function() {
  generateStaticContent();
  addWebUIListener('data-changed', updateEverything);
});
