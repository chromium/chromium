// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {routes} from '../route.js';

import type {SettingsMenuElement} from './settings_menu.js';

export function getHtml(this: SettingsMenuElement) {
  return html`<!--_html_template_start_-->
<div role="navigation">
  <cr-menu-selector id="menu"
      selectable="a:not(#extensionsLink):not(#managementIcon)"
      attr-for-selected="href" @iron-activate="${this.onIronActivate_}"
      @click="${this.onLinkClick_}" selected-attribute="selected">
    ${this.settingsRefresh2026Enabled_ ? html`
      <a role="menuitem" id="home" href="/"
          ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.home)}"
          class="cr-nav-menu-item menu-item">
        <cr-icon icon="${this.getRouteIcon_(routes.BASIC)}"></cr-icon>
        <!-- TODO(b/574662686): Update "Home" string -->
        <span class="menu-label">Home</span>
        <cr-ripple></cr-ripple>
      </a>
    ` : html`
      <a role="menuitem" id="people" href="/people"
          ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.people)}"
          class="cr-nav-menu-item menu-item">
        <cr-icon icon="${this.getRouteIcon_(routes.PEOPLE)}"></cr-icon>
        <span class="menu-label">$i18n{peoplePageTitle}</span>
        <cr-ripple></cr-ripple>
      </a>
    `}
    <a role="menuitem" id="autofill" href="/autofill"
        @click="${this.onAutofillClick_}" class="cr-nav-menu-item menu-item"
        ?hidden="${
            this.shouldHideMenuItem_(this.pageVisibility_?.yourSavedInfo)}">
      <cr-icon icon="${this.getRouteIcon_(routes.AUTOFILL)}"></cr-icon>
      <span class="menu-label">$i18n{autofillPageTitle}</span>
      <cr-ripple></cr-ripple>
    </a>
    <a role="menuitem" id="privacy" href="/privacy"
        ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.privacy)}"
        class="cr-nav-menu-item menu-item">
      <cr-icon icon="${this.getRouteIcon_(routes.PRIVACY)}"></cr-icon>
      <span class="menu-label">$i18n{privacyPageTitle}</span>
      <cr-ripple></cr-ripple>
    </a>
    <a role="menuitem" id="performance" href="/performance"
        class="cr-nav-menu-item menu-item" ?hidden="${
            this.shouldHideMenuItem_(this.pageVisibility_?.performance)}">
      <cr-icon icon="${this.getRouteIcon_(routes.PERFORMANCE)}"></cr-icon>
      <span class="menu-label">$i18n{performancePageTitle}</span>
      <cr-ripple></cr-ripple>
    </a>
    <a role="menuitem" id="ai" href="/ai"
        ?hidden="${!this.showAiPageMenuItem_()}"
        class="cr-nav-menu-item menu-item">
      <cr-icon icon="${this.getRouteIcon_(routes.AI)}"></cr-icon>
      <span class="menu-label">$i18n{aiPageTitle}</span>
      <cr-ripple></cr-ripple>
    </a>
    <a role="menuitem" id="appearance" href="/appearance"
        ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.appearance)}"
        class="cr-nav-menu-item menu-item">
      <cr-icon icon="${this.getRouteIcon_(routes.APPEARANCE)}"></cr-icon>
      <span class="menu-label">$i18n{appearancePageTitle}</span>
      <cr-ripple></cr-ripple>
    </a>
    ${!this.settingsRefresh2026Enabled_ ? html`
      <a role="menuitem" id="search" href="/search"
          class="cr-nav-menu-item menu-item">
        <cr-icon icon="${this.getRouteIcon_(routes.SEARCH)}"></cr-icon>
        <span class="menu-label">$i18n{searchPageTitle}</span>
        <cr-ripple></cr-ripple>
      </a>
<if expr="not is_chromeos">
      <a role="menuitem" id="defaultBrowser" class="cr-nav-menu-item menu-item"
          href="/defaultBrowser" ?hidden="${
              this.shouldHideMenuItem_(this.pageVisibility_?.defaultBrowser)}">
        <cr-icon icon="${this.getRouteIcon_(routes.DEFAULT_BROWSER)}"></cr-icon>
        <span class="menu-label">$i18n{defaultBrowser}</span>
        <cr-ripple></cr-ripple>
      </a>
</if>
      <a role="menuitem" id="onStartup" href="/onStartup"
          class="cr-nav-menu-item menu-item" ?hidden="${
              this.shouldHideMenuItem_(this.pageVisibility_?.onStartup)}">
        <cr-icon icon="${this.getRouteIcon_(routes.ON_STARTUP)}"></cr-icon>
        <span class="menu-label">$i18n{onStartup}</span>
        <cr-ripple></cr-ripple>
      </a>
      <div class="menu-separator"></div>
    ` : ''}
    <a role="menuitem" id="languages" href="/languages"
        class="cr-nav-menu-item menu-item"
        ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.languages)}">
      <cr-icon icon="${this.getRouteIcon_(routes.LANGUAGES)}"></cr-icon>
      <span class="menu-label">$i18n{languagesPageTitle}</span>
      <cr-ripple></cr-ripple>
    </a>
    ${!this.settingsRefresh2026Enabled_ ? html`
      <a role="menuitem" id="downloads" href="/downloads"
          class="cr-nav-menu-item menu-item" ?hidden="${
              this.shouldHideMenuItem_(this.pageVisibility_?.downloads)}">
        <cr-icon icon="${this.getRouteIcon_(routes.DOWNLOADS)}"></cr-icon>
        <span class="menu-label">$i18n{downloadsPageTitle}</span>
        <cr-ripple></cr-ripple>
      </a>
    ` : ''}
    <a role="menuitem" id="accessibility" href="/accessibility"
        class="cr-nav-menu-item menu-item"
        ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.a11y)}">
      <cr-icon icon="${this.getRouteIcon_(routes.ACCESSIBILITY)}"></cr-icon>
      <span class="menu-label">$i18n{a11yPageTitle}</span>
      <cr-ripple></cr-ripple>
    </a>
    ${this.settingsRefresh2026Enabled_ ? html`
      <a role="menuitem" id="search" href="/search"
          class="cr-nav-menu-item menu-item">
        <cr-icon icon="${this.getRouteIcon_(routes.SEARCH)}"></cr-icon>
        <span class="menu-label">$i18n{searchPageTitle}</span>
        <cr-ripple></cr-ripple>
      </a>
      <a role="menuitem" id="general" href="/general"
          class="cr-nav-menu-item menu-item"
          ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.general)}">
        <cr-icon icon="${this.getRouteIcon_(routes.GENERAL)}"></cr-icon>
        <span class="menu-label">$i18n{generalPageTitle}</span>
        <cr-ripple></cr-ripple>
      </a>
    ` : html`
<if expr="not is_chromeos">
      <a role="menuitem" id="system" href="/system"
          class="cr-nav-menu-item menu-item"
          ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.system)}">
        <cr-icon icon="${this.getRouteIcon_(routes.SYSTEM)}"></cr-icon>
        <span class="menu-label">$i18n{systemPageTitle}</span>
        <cr-ripple></cr-ripple>
      </a>
</if>
      <a role="menuitem" id="reset" href="/reset"
          ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.reset)}"
          class="cr-nav-menu-item menu-item">
        <cr-icon icon="${this.getRouteIcon_(routes.RESET)}"></cr-icon>
        <span class="menu-label">$i18n{resetPageTitle}</span>
        <cr-ripple></cr-ripple>
      </a>
    `}
    <div ?hidden="${this.hideBottomMenuSeparator_()}" class="menu-separator">
    </div>
    <a role="menuitem" id="extensionsLink" class="cr-nav-menu-item menu-item"
        href="chrome://extensions" target="_blank"
        ?hidden="${this.shouldHideMenuItem_(this.pageVisibility_?.extensions)}"
        @click="${this.onExtensionsLinkClick_}"
        title="$i18n{extensionsLinkTooltip}">
      <cr-icon icon="settings:chrome-extension"></cr-icon>
      <span class="menu-label">$i18n{extensionsPageTitle}</span>
      <div class="cr-icon icon-external"></div>
      <cr-ripple></cr-ripple>
    </a>
    <a role="menuitem" id="about-menu" href="/help"
        class="cr-nav-menu-item menu-item">
      <cr-icon icon="${this.getRouteIcon_(routes.ABOUT)}"></cr-icon>
      <span class="menu-label">$i18n{aboutPageTitle}</span>
      <cr-ripple></cr-ripple>
    </a>
    ${this.settingsRefresh2026Enabled_ && this.isManaged_ && this.collapsed ?
        html`
      <a role="menuitem" id="managementIcon" href="${this.managementPageUrl_}"
          target="_blank" class="cr-nav-menu-item menu-item"
          aria-label="$i18n{managementPage}">
        <cr-icon icon="${this.managedByIcon_}"></cr-icon>
        <cr-ripple></cr-ripple>
      </a>
    ` : ''}
  </cr-menu-selector>
  ${this.settingsRefresh2026Enabled_ && this.isManaged_ && !this.collapsed ?
      html`
    <div id="management" class="menu-item">
      <cr-icon icon="${this.managedByIcon_}"></cr-icon>
      <span class="menu-label" .innerHTML="${this.getManagementString_()}">
      </span>
    </div>
  ` : ''}
</div>

<!--_html_template_end_-->`;
}
