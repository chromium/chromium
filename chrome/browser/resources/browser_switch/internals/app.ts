// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BrowserSwitchInternalsProxy, Decision, RuleSet, RuleSetList, RulesetSources, TimestampPair} from './browser_switch_internals_proxy.js';
import {BrowserSwitchInternalsProxyImpl} from './browser_switch_internals_proxy.js';

interface XmlSiteListItem {
  policyName: string;
  url: string;
}

interface RuleItem {
  rule: string;
  rulesetName: string;
}

const AppElementBase = I18nMixinLit(CrLitElement);

export class AppElement extends AppElementBase {
  static get is() {
    return 'browser-switch-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isBrowserSwitcherEnabled_: {type: Boolean},
      showSearch_: {type: Boolean},
      lastFetch_: {type: String},
      nextFetch_: {type: String},
      urlCheckerInput_: {type: String},
      urlCheckerOutput_: {type: Array},
      greyListRules_: {type: Array},
      siteListRules_: {type: Array},
      xmlSiteLists_: {type: Array},
    };
  }

  protected isBrowserSwitcherEnabled_: boolean = true;
  protected showSearch_: boolean = false;
  protected greyListRules_: RuleItem[] = [];
  protected siteListRules_: RuleItem[] = [];
  protected xmlSiteLists_: XmlSiteListItem[] = [];
  protected urlCheckerInput_: string = '';
  protected urlCheckerOutput_: string[] = [];
  protected lastFetch_: string = '';
  protected nextFetch_: string = '';

  override firstUpdated() {
    this.updateEverything();

    document.addEventListener('DOMContentLoaded', () => {
      addWebUiListener('data-changed', () => this.updateEverything());
    });
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('urlCheckerInput_')) {
      this.checkUrl_(this.urlCheckerInput_);
    }
  }

  getRuleBrowserName(rule: string) {
    return rule.startsWith('!') ? getBrowserName() : getAltBrowserName();
  }

  getPolicyFromRuleset(ruleSetName: string) {
    const rulesetToPolicy: Record<string, string> = {
      gpo: 'BrowserSwitcherUrlList',
      ieem: 'BrowserSwitcherUseIeSitelist',
      external_sitelist: 'BrowserSwitcherExternalSitelistUrl',
      external_greylist: 'BrowserSwitcherExternalGreylistUrl',
    };
    return rulesetToPolicy[ruleSetName];
  }

  /**
   * Updates the content of all tables after receiving data from the backend.
   */
  updateTables(rulesets: RuleSetList) {
    this.siteListRules_ = [];
    this.greyListRules_ = [];

    const listNameToProperty: Record<string, RuleItem[]> = {
      sitelist: this.siteListRules_,
      greylist: this.greyListRules_,
    };

    for (const [rulesetName, ruleset] of Object.entries(rulesets)) {
      for (const [listName, rules] of Object.entries(ruleset as RuleSet)) {
        listNameToProperty[listName]!.push(...rules.map((rule: string) => ({
                                                          rulesetName,
                                                          rule,
                                                        })));
      }
    }
  }

  /**
   * Takes the json from the url checker and makes it readable.
   */
  urlOutputText(decision: Decision): string[] {
    let opensIn = '';
    const altBrowserName = getAltBrowserName();
    const browserName = getBrowserName();

    switch (decision.action) {
      case 'stay':
        opensIn = this.i18n('openBrowser', browserName) + '\n';
        break;
      case 'go':
        opensIn = this.i18n('openBrowser', altBrowserName) + '\n';
        break;
    }

    let reason = '';
    if (decision.matching_rule) {
      if (decision.matching_rule.startsWith('!')) {
        reason += this.i18n(
                      'openBrowserInvertRuleReason',
                      JSON.stringify(decision.matching_rule)) +
            '\n';
      } else {
        const list = decision.reason === 'sitelist' ?
            this.i18n('forceOpenTitle') :
            this.i18n('ignoreTitle');
        reason += this.i18n(
                      'openBrowserRuleReason',
                      JSON.stringify(decision.matching_rule), list) +
            '\n';
      }
    }
    // if undefined - add nothing to the output

    switch (decision.reason) {
      case 'globally_disabled':
        throw new Error('BrowserSwitcherEnabled policy is set as false!');
      case 'protocol':
        reason += this.i18n('openBrowserProtocolReason') + '\n';
        break;
      case 'default':
        reason += this.i18n('openBrowserDefaultReason', browserName) + '\n';
        break;
    }

    return [opensIn, reason];
  }

  private checkUrl_(url: string) {
    if (!url) {
      this.urlCheckerOutput_ = [];
      return;
    }

    if (!url.includes('://')) {
      url = 'http://' + url;
    }

    getProxy()
        .getDecision(url)
        .then((decision) => {
          // URL is valid.
          this.urlCheckerOutput_ = this.urlOutputText(decision);
        })
        .catch((errorMessage) => {
          // URL is invalid.
          console.warn(errorMessage);
          this.urlCheckerOutput_ = [this.i18n('invalidURL')];
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
    const {last_fetch, next_fetch} = timestamps;

    this.lastFetch_ = last_fetch !== 0 ? formatTime(last_fetch) : '';
    this.nextFetch_ = next_fetch !== 0 ? formatTime(next_fetch) : '';
  }

  /**
   * Update the table under the "XML sitelists" section.
   */
  updateXmlTable(rulesetSources: RulesetSources) {
    this.xmlSiteLists_ =
        Object.entries(rulesetSources)
            .map(([prefName, url]) => ({
                   // Hacky name guessing
                   policyName: 'BrowserSwitcher' +
                       snakeCaseToUpperCamelCase(prefName.split('.')[1]!),
                   url: url || this.i18n('notConfigured'),
                 }));
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
    }
  }

  /**
   * Section: XML configuration source
   * Shows information about the last time XML sitelists were downloaded.
   */
  protected getXmlSitelistsLastDownloadLabel(): string {
    return this.i18n('xmlSitelistLastDownloadDate', this.lastFetch_);
  }

  /**
   * Section: XML configuration source
   * Shows information about the next download time of XML sitelists.
   */
  protected getXmlSitelistsNextDownloadLabel(): string {
    return this.i18n('xmlSitelistNextDownloadDate', this.nextFetch_);
  }

  /**
   * Section: Ignore
   * Paragraph that informs that the URLs that are affected by the lists
   * BrowserSwitcherExternalGreylistUrl and BrowserSwitcherUrlGreylist
   * will not trigger a browser switch.
   */
  protected getIgnoreUrlMatchingLabel(): string {
    return this.i18n('ignoreParagraph2', getBrowserName(), getAltBrowserName());
  }

  protected onUrlCheckerInputInput_(e: Event) {
    this.urlCheckerInput_ = (e.target as CrInputElement).value;
  }
}

customElements.define(AppElement.is, AppElement);

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
function formatTime(dateNumber: number): string {
  const date = new Date(dateNumber);
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
