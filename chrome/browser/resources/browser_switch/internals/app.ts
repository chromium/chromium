// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserSwitchInternalsProxy, BrowserSwitchInternalsProxyImpl, Decision, RuleSet, RuleSetList, RulesetSources, TimestampPair} from './browser_switch_internals_proxy.js';

type ListName = 'sitelist'|'greylist';

const BrowserSwitchInternalsAppElementBase = PolymerElement;

class BrowserSwitchInternalsAppElement extends
    BrowserSwitchInternalsAppElementBase {
  static get is() {
    return 'browser-switch-internals-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      isBrowserSwitcherEnabled_: {
        type: Boolean,
        value: true,
      },
      showSearch_: {
        type: Boolean,
        value: false,
      }
    };
  }

  private isBrowserSwitcherEnabled_: boolean;
  private showSearch_: boolean;

  /** @override */
  ready() {
    super.ready();

    this.updateEverything();

    document.addEventListener('DOMContentLoaded', () => {
      this.generateStaticContent();
      addWebUIListener('data-changed', () => this.updateEverything());
    });
  }

  /**
   * Clears the table, and inserts a header row.
   * @param headerTemplate Template to use to re-create the header row.
   */
  clearTable(table: HTMLTableElement, headerTemplate: HTMLTemplateElement) {
    table.innerHTML = '';
    const headerRow = document.importNode(headerTemplate.content, true);
    table.appendChild(headerRow);
  }

  /**
   * Creates and returns a <tr> element for the given rule.
   */
  createRowForRule(rule: string, rulesetName: string, listType: ListName):
      HTMLTableRowElement {
    const templateName = `rule-row-template-${listType}`;
    const row = document.importNode(
                    (this.$[templateName] as HTMLTemplateElement).content,
                    true) as unknown as HTMLTableRowElement;
    const cells = row.querySelectorAll('td');
    cells[0].innerText = rule;
    cells[0].className = 'url';
    if (listType === 'sitelist') {
      cells[1].innerText =
          rule.startsWith('!') ? getBrowserName() : getAltBrowserName();
      cells[2].appendChild(
          this.createRulesetColumnWithTooltip(rulesetName, listType));
    } else if (listType === 'greylist') {
      cells[1].appendChild(
          this.createRulesetColumnWithTooltip(rulesetName, listType));
    }

    return row;
  }

  createRulesetColumnWithTooltip(rulesetName: string, listType: ListName):
      HTMLSpanElement {
    const textSpan = document.createElement('span');
    textSpan.className = 'tooltip';
    textSpan.innerText = rulesetName;
    const tooltipSpan = document.createElement('span');
    tooltipSpan.className = 'right';
    const rulesetToPolicy: Record<ListName, Record<string, string>> = {
      'sitelist': {
        'gpo': 'BrowserSwitcherUrlList',
        'ieem': 'BrowserSwitcherUseIeSitelist',
        'external': 'BrowserSwitcherExternalSitelistUrl',
      },
      'greylist': {
        'gpo': 'BrowserSwitcherUrlGreylist',
        'external': 'BrowserSwitcherExternalGreylistUrl',
      }
    };
    tooltipSpan.innerText = rulesetToPolicy[listType][rulesetName];
    textSpan.appendChild(tooltipSpan);
    return textSpan;
  }

  /**
   * Updates the content of all tables after receiving data from the backend.
   */
  updateTables(rulesets: RuleSetList) {
    const siteListHeaderTemplate =
        this.$['header-row-template-sitelist'] as HTMLTemplateElement;
    const greyListHeaderTemplate =
        this.$['header-row-template-greylist'] as HTMLTemplateElement;
    this.clearTable(
        this.$['sitelist'] as HTMLTableElement, siteListHeaderTemplate);
    this.clearTable(
        this.$['greylist'] as HTMLTableElement, greyListHeaderTemplate);

    for (const [rulesetName, ruleset] of Object.entries(rulesets)) {
      for (const [listName, rules] of Object.entries(ruleset as RuleSet)) {
        const table = this.$[listName];
        for (const rule of rules) {
          table.appendChild(
              this.createRowForRule(rule, rulesetName, listName as ListName));
        }
      }
    }
  }

  /**
   * Takes the json from the url checker and makes it readable.
   */
  urlOutputText(decision: Decision): Array<string> {
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
        reason += `Reason: The inverted rule ${
            JSON.stringify(decision.matching_rule)} was found in `;
      } else {
        reason +=
            `Reason: ${JSON.stringify(decision.matching_rule)} was found in `;
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

  checkUrl() {
    let url = (this.$['url-checker-input'] as HTMLInputElement).value;
    if (!url) {
      (this.$['output'] as HTMLUListElement).innerText = '';
      return;
    }
    if (!url.includes('://')) {
      url = 'http://' + url;
    }
    getProxy()
        .getDecision(url)
        .then((decision) => {
          // URL is valid.
          const [output, reason] = this.urlOutputText(decision);
          (this.$['output'] as HTMLUListElement).innerText = output;
          (this.$['reason'] as HTMLUListElement).innerText = reason;
        })
        .catch((errorMessage) => {
          // URL is invalid.
          console.warn(errorMessage);
          (this.$['output'] as HTMLUListElement).innerText =
              'Invalid URL. Make sure it is formatted properly.';
        });
  }

  refreshXml() {
    getProxy().refreshXml();
  }

  /**
   * Update the paragraphs under the "XML sitelists" section.
   */
  updateTimestamps(timestamps: TimestampPair|null) {
    if (!timestamps) {
      return;
    }

    const lastFetch = new Date(timestamps.last_fetch);
    const nextFetch = new Date(timestamps.next_fetch);

    if (lastFetch.valueOf() == 0) {
      // Not fetched yet.
      (this.$['xml-not-fetched-yet'] as HTMLParagraphElement).style.display =
          'block';
      (this.$['xml-last-fetch'] as HTMLParagraphElement).style.display = 'none';
    } else {
      // Already fetched.
      (this.$['xml-not-fetched-yet'] as HTMLParagraphElement).style.display =
          'none';
      (this.$['xml-last-fetch'] as HTMLParagraphElement).style.display =
          'block';
    }

    (this.$['xml-next-fetch'] as HTMLParagraphElement).style.display =
        nextFetch.valueOf() ? 'block' : 'none';

    (this.$['last-fetch-placeholder'] as HTMLSpanElement).innerText =
        formatTime(lastFetch);
    (this.$['next-fetch-placeholder'] as HTMLSpanElement).innerText =
        formatTime(nextFetch);
  }

  /**
   * Update the table under the "XML sitelists" section.
   */
  updateXmlTable({browser_switcher: sources}: RulesetSources) {
    const headerTemplate =
        this.$['xml-header-row-template'] as HTMLTemplateElement;
    this.clearTable(
        this.$['xml-sitelists'] as HTMLTableElement, headerTemplate);

    for (const [prefName, url] of Object.entries(sources)) {
      // Hacky: guess the policy name from the pref name by converting 'foo_bar'
      // to 'BrowserSwitcherFooBar'. This relies on prefs having the same name
      // as the associated policy.
      const policyName =
          'BrowserSwitcher' + snakeCaseToUpperCamelCase(prefName);
      const row = document.importNode(
          (this.$['xml-row-template'] as HTMLTemplateElement).content, true);
      const cells = row.querySelectorAll('td');
      cells[0].innerText = policyName;
      cells[1].innerText = url || '(not configured)';
      cells[1].className = 'url';
      this.$['xml-sitelists'].appendChild(row);
    }

    // Hide/show the description paragraphs depending on whether any XML
    // sitelist is configured.
    const enabled = Object.values(sources).some(x => !!x);
    (this.$['xml-description-wrapper'] as HTMLDivElement).style.display =
        enabled ? 'block' : 'none';
  }

  // TODO(crbug.com/1258133): Make `browserName' and `altBrowserName' Polymer
  // properties, and replace this method with HTML.
  generateStaticContent() {
    (this.$['greylist-description'] as HTMLParagraphElement).innerText =
        `URLs matching these rules won't trigger a browser switch and can be open in either ${
            getBrowserName()} or ${getAltBrowserName()}.`;
  }

  /**
   * Called by C++ when we need to update everything on the page.
   */
  async updateEverything() {
    this.isBrowserSwitcherEnabled_ =
        await getProxy().isBrowserSwitcherEnabled();
    if (this.isBrowserSwitcherEnabled_) {
      getProxy().getAllRulesets().then(
          (rulesets) => this.updateTables(rulesets));
      getProxy().getTimestamps().then(
          (timestamps) => this.updateTimestamps(timestamps));
      getProxy().getRulesetSources().then(
          (sources) => this.updateXmlTable(sources));
      this.checkUrl();
    }
  }
}

customElements.define(
    BrowserSwitchInternalsAppElement.is, BrowserSwitchInternalsAppElement);

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
 * Formats |date| as "HH:MM:SS".
 */
function formatTime(date: Date): string {
  const hh = date.getHours().toString().padStart(2, '0');
  const mm = date.getMinutes().toString().padStart(2, '0');
  const ss = date.getSeconds().toString().padStart(2, '0');
  return `${hh}:${mm}:${ss}`;
}

/**
 * Gets the English name of the alternate browser.
 */
function getAltBrowserName(): string {
  // TODO (crbug.com/1258133): if you change the AlternativeBrowserPath
  // policy, then loadTimeData can contain stale data. It won't update
  // until you refresh (despite the rest of the page auto-updating).
  return loadTimeData.getString('altBrowserName') || 'alternative browser';
}

/**
 * Gets the English name of the browser.
 */
function getBrowserName(): string {
  // TODO (crbug.com/1258133): if you change the AlternativeBrowserPath
  // policy, then loadTimeData can contain stale data. It won't update
  // until you refresh (despite the rest of the page auto-updating).
  return loadTimeData.getString('browserName');
}

function getProxy(): BrowserSwitchInternalsProxy {
  return BrowserSwitchInternalsProxyImpl.getInstance();
}
