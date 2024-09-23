// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-toolbar
    page-name="$i18n{switchInternalTitle}"
    clear-label="clear"
    role="banner"
    show-search="${this.showSearch_}">
</cr-toolbar>

<div class="container">
  <div class="card">
    <p class="card-text">
      $i18n{switchInternalDescription}
    </p>
  </div>
</div>

<div class="container" ?hidden="${this.isBrowserSwitcherEnabled_}">
  <section class="card">
    <h2>$i18n{nothingShown}</h2>

    <p>$i18nRaw{switcherDisabled}</p>
  </section>
</div>

<div class="container" ?hidden="${!this.isBrowserSwitcherEnabled_}">
  <div class="row-container">
    <section class="url-card-container">
      <h2>$i18n{urlCheckerTitle}</h2>

      <label>$i18n{urlCheckerDesc}</label>
      <label>
        <cr-input
            type="text"
            .value="${this.urlCheckerInput_}"
            @input="${this.onUrlCheckerInputInput_}"
            placeholder="http://example.com/">
        </cr-input>
      </label>

      <ul>
        ${this.urlCheckerOutput_.map(item => html`
          <li>${item}</li>
        `)}
      </ul>
    </section>
    <section class="cr-left-card-container">
      <h2 class="tooltip">
        $i18n{xmlTitle}
        <span class="right">$i18n{xmlDesc}</span>
      </h2>

      <table>
        <tr>
          <th>$i18n{xmlSource}</th>
          <th>URL</th>
        </tr>
        ${this.xmlSiteLists_.map(item => html`
          <tr>
            <td>${item.policyName}</td>
            <td class="url">${item.url}</td>
          </tr>
        `)}
      </table>

      ${this.xmlSiteLists_.length ? html`
        <p>
          ${!this.lastFetch_ ? html`
            $i18n{sitelistNotFetched}
          ` : html`
            ${this.getXmlSitelistsLastDownloadLabel()}
          `}
        </p>
        ${this.nextFetch_ ? html`
          <p>${this.getXmlSitelistsNextDownloadLabel()}</p>
        ` : ''}
        <p>
          <cr-button @click="${this.refreshXml}">
            $i18n{sitelistDownloadButton}
          </cr-button>
        </p>
      ` : ''}
    </section>
    <section class="cr-left-card-container">
      <h2 class="tooltip">
        $i18n{forceOpenTitle}
        <span class="right">$i18n{forceOpenDescription}</span>
      </h2>
      <h4>$i18nRaw{forceOpenParagraph1}</h4>

      <p>$i18n{forceOpenParagraph2}</p>
      <table>
        <tr>
          <th>$i18n{forceOpenTableColumnRule}</th>
          <th>$i18n{forceOpenTableColumnOpensIn}</th>
          <th>$i18n{forceOpenTableColumnSource}</th>
        </tr>
        ${this.siteListRules_.map(item => html`
          <tr>
            <td class="url">${item.rule}</td>
            <td>${this.getRuleBrowserName(item.rule)}</td>
            <td>
              <span class="tooltip">
                ${item.rulesetName}
                <span class="right">
                  ${this.getPolicyFromRuleset(item.rulesetName)}
                </span>
              </span>
            </td>
          </tr>
        `)}
      </table>
    </section>
    <section class="cr-left-card-container">
      <h2 class="tooltip">
        $i18n{ignoreTitle}
        <span class="right">$i18n{ignoreDescription}</span>
      </h2>
      <h4>$i18nRaw{ignoreParagraph1}</h4>
      <p>${this.getIgnoreUrlMatchingLabel()}</p>
      <table>
        <tr>
          <th>$i18n{ignoreTableColumnRule}</th>
          <th>$i18n{ignoreTableColumnSource}</th>
        </tr>
        ${this.greyListRules_.map(item => html`
          <tr>
            <td class="url">${item.rule}</td>
            <td>
              <span class="tooltip">
                ${item.rulesetName}
                <span class="right">
                  ${this.getPolicyFromRuleset(item.rulesetName)}
                </span>
              </span>
            </td>
          </tr>
        `)}
      </table>
    </section>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
