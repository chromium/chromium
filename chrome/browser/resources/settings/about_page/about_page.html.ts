// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {RestartType} from '../relaunch_mixin_lit.js';

import {getHtml as getActionRowsHtml} from './about_page_action_rows.html.js';
// clang-format off
// <if expr="_google_chrome">
import {ABOUT_PAGE_PRIVACY_POLICY_URL} from './about_page.js';
// </if>
// clang-format on
import type {SettingsAboutPageElement} from './about_page.js';

export function getHtml(this: SettingsAboutPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{aboutPageTitle}">
  ${!this.settingsRefresh2026Enabled_ ? html`
    <div class="cr-row two-line first">
      <img id="productLogo" @click="${this.onProductLogoClick_}"
          srcset="chrome://theme/current-channel-logo@1x 1x,
                chrome://theme/current-channel-logo@2x 2x"
          alt="$i18n{aboutProductLogoAlt}" role="presentation">
      <div class="product-title">$i18n{aboutProductTitle}</div>
    </div>
  ` : ''}
  <div
      class="cr-row two-line ${
          this.settingsRefresh2026Enabled_ ? 'first' : ''}">
<if expr="not is_chromeos">
    ${this.settingsRefresh2026Enabled_ ? html`
      <img id="throbber" src="chrome://resources/images/throbber_small.svg"
          ?hidden="${!this.shouldShowThrobber_()}">
    ` : html`
      <!-- Set the icon from the iconset (when it's obsolete/EOL and
        when update is done) or set the src (when it's updating). -->
      <div class="icon-container" ?hidden="${!this.shouldShowIcons_()}">
        <cr-icon ?hidden="${this.shouldShowThrobber_()}"
            icon="${this.getUpdateStatusIcon_()}">
        </cr-icon>
        <img id="throbber" src="chrome://resources/images/throbber_small.svg"
            ?hidden="${!this.shouldShowThrobber_()}">
      </div>
    `}
</if>
    ${this.settingsRefresh2026Enabled_ ? html`
      <img id="productLogo" @click="${this.onProductLogoClick_}"
          srcset="chrome://theme/current-channel-logo@1x 1x,
                chrome://theme/current-channel-logo@2x 2x"
          alt="$i18n{aboutProductLogoAlt}" role="presentation"
          ?hidden="${this.shouldShowThrobber_()}">
    ` : ''}
    <div class="flex cr-padded-text">
<if expr="not is_chromeos">
      <div id="updateStatusMessage" class="heading"
          ?hidden="${!this.showUpdateStatus_}">
        <div role="alert" aria-live="polite"
            .innerHTML="${this.getUpdateStatusMessage_()}">
        </div>
        <a class="secondary" ?hidden="${!this.shouldShowLearnMoreLink_()}"
            target="_blank"
            href="https://support.google.com/chrome?p=update_error"
            aria-label="$i18nPolymer{aboutLearnMoreUpdatingErrors}">
          $i18n{learnMore}
        </a>
      </div>
      <span id="deprecationWarning" class="heading"
          ?hidden="${!this.obsoleteSystemInfo_.obsolete}">
        $i18n{aboutObsoleteSystem}
        <a class="secondary" href="$i18n{aboutObsoleteSystemURL}"
            target="_blank"
            aria-label="$i18nPolymer{aboutLearnMoreSystemRequirements}">
          $i18n{learnMore}
        </a>
      </span>
</if>
      <div class="secondary">$i18n{aboutBrowserVersion}</div>
    </div>
<if expr="not is_chromeos">
    <div class="separator" ?hidden="${!this.showButtonContainer_}"></div>
    <span id="buttonContainer" ?hidden="${!this.showButtonContainer_}">
      <cr-button id="relaunch" ?hidden="${!this.showRelaunch_}"
          @click="${this.onRelaunchClick_}">
        $i18n{aboutRelaunch}
      </cr-button>
    </span>
</if>
  </div>
  ${!this.settingsRefresh2026Enabled_ ? getActionRowsHtml.bind(this)() : ''}
</settings-section>

${this.settingsRefresh2026Enabled_ ? html`
  <settings-section>
    ${getActionRowsHtml.bind(this)()}
  </settings-section>
` : ''}

<settings-section>
  <div class="info-sections">
    <div class="info-section">
      <div id="footerProductTitle" class="heading">
        $i18n{aboutProductTitle}
      </div>
      <div class="secondary">$i18n{aboutProductCopyright}</div>
    </div>

    <div class="info-section">
      <div class="secondary">$i18nRaw{aboutProductLicense}</div>
    </div>
<if expr="_google_chrome or _is_chrome_for_testing_branded">
    <div class="secondary">
<if expr="_google_chrome">
      ${this.settingsRefresh2026Enabled_ ? html`
        <a id="privacyPolicy" href="${ABOUT_PAGE_PRIVACY_POLICY_URL}"
            target="_blank" aria-description="$i18n{opensInNewTab}"><!--
          -->$i18n{aboutPrivacyPolicy}<!--
        --></a>
        <span aria-hidden="true">|</span>
      ` : ''}
</if>
      <a id="tos" href="$i18n{aboutTermsURL}">$i18n{aboutProductTos}</a>
    </div>
</if>
<if expr="not is_chromeos">
    ${this.shouldShowRelaunchDialog ? html`
      <relaunch-confirmation-dialog .restartType="${RestartType.RELAUNCH}"
          @close="${this.onRelaunchDialogClose}">
      </relaunch-confirmation-dialog>
    ` : ''}
</if>
  </div>
</settings-section>
<!--_html_template_end_-->`;
}
