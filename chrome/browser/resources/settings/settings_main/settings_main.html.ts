// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {cache, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsMainElement} from './settings_main.js';

export function getHtml(this: SettingsMainElement) {
  return html`<!--_html_template_start_-->
<div id="noSearchResults" ?hidden="${!this.showNoResultsFound_}">
  <div>$i18n{searchNoResults}</div>
  <div>$i18nRaw{searchNoResultsHelp}</div>
</div>
${this.showManagedHeader_() ? html`
  <managed-footnote></managed-footnote>
` : ''}

${this.showResetProfileBanner_ ? html`
  <settings-reset-profile-banner @close="${this.onResetProfileBannerClose_}">
  </settings-reset-profile-banner>
` : ''}

<cr-view-manager id="switcher" ?show-all="${this.shouldShowAll_()}">
  ${cache(this.showPage_(this.pageVisibility_?.people) ? html`
    <div slot="view" id="people">
      ${cache(this.settingsRefresh2026Enabled_ &&
          this.renderPlugin_(this.routes_.PEOPLE) ? html`
        <div>Home page placeholder</div>
      ` : '')}
      ${cache(!this.settingsRefresh2026Enabled_ &&
          this.renderPlugin_(this.routes_.PEOPLE) ? html`
        <settings-people-page-index ?in-search-mode="${this.inSearchMode_}">
        </settings-people-page-index>
      ` : '')}
    </div>
  ` : '')}

  ${cache(this.showPage_(this.pageVisibility_?.privacy) ? html`
    <div slot="view" id="privacy">
      ${cache(this.renderPlugin_(this.routes_.PRIVACY) ? html`
        <settings-privacy-page-index ?in-search-mode="${this.inSearchMode_}">
        </settings-privacy-page-index>
      ` : '')}
    </div>
  ` : '')}

  ${cache(this.showPage_(this.pageVisibility_?.yourSavedInfo) ? html`
    <div slot="view" id="yourSavedInfo">
      ${cache(this.renderPlugin_(this.routes_.AUTOFILL) ? html`
        <settings-autofill-page-index ?in-search-mode="${this.inSearchMode_}">
        </settings-autofill-page-index>
      ` : '')}
    </div>
  ` : '')}

  ${cache(this.showPage_(this.pageVisibility_?.performance) ? html`
    <div slot="view" id="performance">
      ${cache(this.renderPlugin_(this.routes_.PERFORMANCE) ? html`
        <settings-performance-page-index ?in-search-mode="${this.inSearchMode_}">
        </settings-performance-page-index>
      ` : '')}
    </div>
  ` : '')}

  ${cache(this.showAiPage_(this.pageVisibility_?.ai) ? html`
    <div slot="view" id="ai">
      ${cache(this.renderPlugin_(this.routes_.AI) ? html`
        <settings-ai-page-index ?in-search-mode="${this.inSearchMode_}">
        </settings-ai-page-index>
      ` : '')}
    </div>
  ` : '')}

  ${cache(this.showPage_(this.pageVisibility_?.appearance) ? html`
    <div slot="view" id="appearance">
      ${cache(this.renderPlugin_(this.routes_.APPEARANCE) ? html`
        <settings-appearance-page-index ?in-search-mode="${this.inSearchMode_}">
        </settings-appearance-page-index>
      ` : '')}
    </div>
  ` : '')}

  <div slot="view" id="search">
    ${cache(this.renderPlugin_(this.routes_.SEARCH) ? html`
      <settings-search-page-index ?in-search-mode="${this.inSearchMode_}">
      </settings-search-page-index>
    ` : '')}
  </div>

<if expr="not is_chromeos">
  ${cache(!this.settingsRefresh2026Enabled_ &&
      this.showPage_(this.pageVisibility_?.defaultBrowser) ? html`
    <div slot="view" id="defaultBrowser">
      ${cache(this.renderPlugin_(this.routes_.DEFAULT_BROWSER) ? html`
        <settings-default-browser-page></settings-default-browser-page>
      ` : '')}
    </div>
  ` : '')}
</if>

  ${cache(!this.settingsRefresh2026Enabled_ &&
      this.showPage_(this.pageVisibility_?.onStartup) ? html`
    <div slot="view" id="onStartup">
      ${cache(this.renderPlugin_(this.routes_.ON_STARTUP) ? html`
        <settings-on-startup-page></settings-on-startup-page>
      ` : '')}
    </div>
  ` : '')}

  ${cache(this.showPage_(this.pageVisibility_?.languages) ? html`
    <div slot="view" id="languages">
      ${cache(this.renderPlugin_(this.routes_.LANGUAGES) ? html`
<if expr="is_chromeos">
        <settings-languages-page-index-cros>
        </settings-languages-page-index-cros>
</if>
<if expr="not is_chromeos">
        <settings-languages-page-index
            ?in-search-mode="${this.inSearchMode_}">
        </settings-languages-page-index>
</if>
      ` : '')}
    </div>
  ` : '')}

  ${cache(!this.settingsRefresh2026Enabled_ &&
      this.showPage_(this.pageVisibility_?.downloads) ? html`
    <div slot="view" id="downloads">
      ${cache(this.renderPlugin_(this.routes_.DOWNLOADS) ? html`
        <settings-downloads-page></settings-downloads-page>
      ` : '')}
    </div>
  ` : '')}

  ${cache(this.showPage_(this.pageVisibility_?.a11y) ? html`
    <div slot="view" id="a11y">
      ${cache(this.renderPlugin_(this.routes_.ACCESSIBILITY) ? html`
        <settings-a11y-page-index ?in-search-mode="${this.inSearchMode_}">
        </settings-a11y-page-index>
      ` : '')}
    </div>
  ` : '')}

  ${cache(this.settingsRefresh2026Enabled_ &&
      this.showPage_(this.pageVisibility_?.general) ? html`
    <div slot="view" id="general">General page placeholder</div>
  ` : '')}

<if expr="not is_chromeos">
  ${cache(!this.settingsRefresh2026Enabled_ &&
      this.showPage_(this.pageVisibility_?.system) ? html`
    <div slot="view" id="system">
      ${cache(this.renderPlugin_(this.routes_.SYSTEM) ? html`
        <settings-system-page></settings-system-page>
      ` : '')}
    </div>
  ` : '')}
</if>

  ${cache(!this.settingsRefresh2026Enabled_ &&
      this.showPage_(this.pageVisibility_?.reset) ? html`
    <div slot="view" id="reset">
      ${cache(this.renderPlugin_(this.routes_.RESET) ? html`
        <settings-reset-page></settings-reset-page>
      ` : '')}
    </div>
  ` : '')}

  <div slot="view" id="about">
    ${cache(this.renderPlugin_(this.routes_.ABOUT) ? html`
      <settings-about-page role="main" class="cr-centered-card-container">
      </settings-about-page>
    ` : '')}
  </div>
</cr-view-manager>
<!--_html_template_end_-->`;
}
