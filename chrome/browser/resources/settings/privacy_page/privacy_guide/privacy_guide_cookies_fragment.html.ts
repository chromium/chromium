// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ThirdPartyCookieBlockingSetting} from '../../site_settings/site_settings_browser_proxy.js';

import type {PrivacyGuideCookiesFragmentElement} from './privacy_guide_cookies_fragment.js';

export function getHtml(this: PrivacyGuideCookiesFragmentElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="settings-fragment-header" focus-element tabindex="-1">
  <picture>
    <source
        srcset="./images/privacy_guide/cookies_graphic_dark_v2.svg"
        media="(prefers-color-scheme: dark)">
    <img alt="" src="./images/privacy_guide/cookies_graphic_v2.svg">
  </picture>
  <h2 class="settings-fragment-header-label">
    $i18n{privacyGuideCookiesCardHeader}
  </h2>
</div>
<div class="fragment-content">
  <settings-radio-group id="cookiesRadioGroup"
      pref-key="generated.third_party_cookie_blocking_setting"
      selectable-elements="settings-collapse-radio-button">
    <settings-collapse-radio-button id="allow3pcs"
        pref-key="generated.third_party_cookie_blocking_setting"
        name="${ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY}"
        label="$i18n{privacyGuideCookiesCardBlockTpcAllowSubheader}"
        expand-aria-label="$i18n{allowThirdPartyCookiesExpandA11yLabel}"
        @click="${this.onCookies3pIncognitoClick_}">
      <div slot="collapse" class="settings-columned-section">
        <div class="column">
          <h3 class="description-header">$i18n{columnHeadingWhenOn}</h3>
          <ul class="icon-bulleted-list">
            <li>
              <cr-icon icon="settings20:account-circle-filled"
                  aria-hidden="true"></cr-icon>
              <div class="secondary">
                $i18n{privacyGuideCookieSettingsAllowWhenOnBulletOne}
              </div>
            </li>
            <li>
              <cr-icon icon="settings20:web" aria-hidden="true"></cr-icon>
              <div class="secondary">
                $i18n{privacyGuideCookieSettingsAllowWhenOnBulletTwo}
              </div>
            </li>
          </ul>
        </div>
        <div class="column">
          <h3 class="description-header">$i18n{columnHeadingConsider}</h3>
          <ul class="icon-bulleted-list">
            <li>
              <cr-icon icon="settings20:user-attributes-filled"
                  aria-hidden="true"></cr-icon>
              <div class="secondary">
                $i18n{privacyGuideCookieSettingsAllowThingsToConsiderBulletOne}
              </div>
            </li>
            <li>
              <cr-icon icon="settings20:incognito" aria-hidden="true">
              </cr-icon>
              <div class="secondary">
                $i18n{privacyGuideCookieSettingsAllowThingsToConsiderBulletTwo}
              </div>
            </li>
          </ul>
        </div>
      </div>
    </settings-collapse-radio-button>
    <settings-collapse-radio-button id="block3pcs"
        pref-key="generated.third_party_cookie_blocking_setting"
        name="${ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY}"
        label="$i18n{privacyGuideCookiesCardBlockTpcBlockSubheader}"
        expand-aria-label="$i18n{blockThirdPartyCookiesExpandA11yLabel}"
        @click="${this.onCookies3pClick_}">
      <div slot="collapse" class="settings-columned-section">
        <div class="column">
          <h3 class="description-header">$i18n{columnHeadingWhenOn}</h3>
          <ul class="icon-bulleted-list">
            <li>
              <cr-icon icon="settings20:block" aria-hidden="true">
              </cr-icon>
              <div class="secondary">
                $i18n{privacyGuideCookieSettingsBlockWhenOnBulletOne}
              </div>
            </li>
            <li>
              <cr-icon icon="settings20:broken-image"
                  aria-hidden="true"></cr-icon>
              <div class="secondary">
                $i18n{privacyGuideCookieSettingsBlockWhenOnBulletTwo}
              </div>
            </li>
          </ul>
        </div>
        <div class="column">
          <h3 class="description-header">$i18n{columnHeadingConsider}</h3>
          <ul class="icon-bulleted-list">
            <li>
              <cr-icon icon="settings:domain-verification" aria-hidden="true">
              </cr-icon>
              <div class="secondary">
                $i18n{privacyGuideCookieSettingsBlockThingsToConsiderBulletOne}
              </div>
            </li>
            <li>
              <cr-icon icon="settings20:fact-check" aria-hidden="true">
              </cr-icon>
              <div class="secondary">
                $i18n{privacyGuideCookieSettingsBlockThingsToConsiderBulletTwo}
              </div>
            </li>
          </ul>
        </div>
      </div>
    </settings-collapse-radio-button>
  </settings-radio-group>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
