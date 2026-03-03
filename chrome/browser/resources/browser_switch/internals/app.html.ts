// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<header>
  <div class="header-left">
    <h1>$i18n{switchInternalTitle}</h1>
  </div>
  <div class="header-right">
    <cr-button @click="${this.onExportToJsonClick_}">
      $i18n{exportToJsonButton}
    </cr-button>
  </div>
</header>

<div class="container">
  <div class="description-container">
    <p>
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
  <!-- Row 1: URL Checker -->
  <div class="url-checker-row">
    <h2>$i18n{urlCheckerTitle}</h2>
    <label>$i18n{urlCheckerDesc}</label>
    <cr-input
        type="text"
        .value="${this.urlCheckerInput_}"
        @input="${this.onUrlCheckerInputInput_}"
        placeholder="http://example.com/">
    </cr-input>
    <ul>
      ${this.urlCheckerOutput_.map(item => html`
        <li>${item}</li>
      `)}
    </ul>
  </div>

  <!-- Row 2: Sitelists -->
  <div class="sitelists-container">
    <section class="card">
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
    <section class="card">
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

  <!-- Row 3: XML Config -->
  <div class="xml-config-row" ?hidden="${!this.showXmlRow}">
    <section class="card">
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
          <cr-button @click="${this.onRefreshXmlClick}">
            $i18n{sitelistDownloadButton}
          </cr-button>
        </p>
      ` : ''}
    </section>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
