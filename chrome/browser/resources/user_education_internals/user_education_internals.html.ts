// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UserEducationInternalsElement} from './user_education_internals.ts';

export function getHtml(this: UserEducationInternalsElement) {
  // clang-format off
  return html`
<cr-toolbar id="toolbar" page-name="IPH Tester"
    clear-label="Clear Search"
    search-prompt="Search IPH and Tutorials"
    autofocus
    @search-changed="${this.onSearchChanged_}"
    role="banner"
    @narrow-changed="${this.onNarrowChanged_}"
    narrow-threshold="920">
</cr-toolbar>
<div id="container" class="cr-scrollable">
  <div class="cr-scrollable-top-shadow"></div>
  <div id="left" ?hidden="${this.narrow_}">
    <div role="navigation">
      <h2>Navigation</h2>
      <cr-menu-selector id="menu" selectable="a" attr-for-selected="href"
          @iron-activate="${this.onSelectorIronActivate_}"
          @click="${this.onLinkClick_}"
          selected-attribute="selected">
        <a role="menuitem" href="#iph" class="cr-nav-menu-item">
          Feature Promos
        </a>
        <a role="menuitem" href="#tutorials" class="cr-nav-menu-item">
          Tutorials
        </a>
        <a role="menuitem" href="#newBadges" class="cr-nav-menu-item">
          "New" Badges
        </a>
        <a role="menuitem" href="#whatsNew" class="cr-nav-menu-item">
          What's New
        </a>
        <a role="menuitem" href="#ntpPromos" class="cr-nav-menu-item">
          NTP Promos
        </a>
        <a role="menuitem" href="#advanced" class="cr-nav-menu-item">
          Advanced
        </a>
      </cr-menu-selector>
    </div>
  </div>
  <div id="main">
    <cr-toast id="errorMessageToast" duration="5000">
      <cr-icon id="errorMessageIcon" class="error-outline"
          icon="cr:error-outline">
      </cr-icon>
      <span id="errorMessage">${this.featurePromoErrorMessage_}</span>
    </cr-toast>
    <div id="content">
      <div id="warning">
        <h2>User Education Debug Page</h2>
        <p>
          <span class="blurb-warning">NOTICE: DEBUGGING PAGE ONLY!</span>
          This page is not part of the intended Chrome experience. It is only
          for testing User Education features and may cause Chrome to behave
          in ways it was not designed to. Use at your own risk.
        </p>
      </div>
      <div id="iph">
        <a name="iph"></a>
        <h2>Feature Promos</h2>
        ${this.featurePromos_.map(item => html`
          <user-education-internals-card
              id="${item.internalName}"
              ?hidden="${!this.promoFilter_(item)}"
              .promo="${item}"
              show-action
              @promo-launch="${this.onFeaturePromoPromoLaunch_}"
              @clear-promo-data="${this.onFeaturePromoClearPromoData_}">
          </user-education-internals-card>`)}
      </div>
      <div id="tutorials">
        <a name="tutorials"></a>
        <h2>Tutorials</h2>
        ${this.tutorials_.map(item => html`
          <user-education-internals-card
              id="${item.internalName}"
              ?hidden="${!this.promoFilter_(item)}"
              .promo="${item}"
              show-action
              @promo-launch="${this.onTutorialPromoLaunch_}">
          </user-education-internals-card>`)}
      </div>
      <div id="newBadges">
        <a name="newBadges"></a>
        <h2>"New" Badges</h2>
        ${this.newBadges_.map(item => html`
          <user-education-internals-card
              id="${item.internalName}"
              ?hidden="${!this.promoFilter_(item)}"
              .promo="${item}"
              @clear-promo-data="${this.onNewBadgeClearPromoData_}">
          </user-education-internals-card>`)}
      </div>
      <div id="ntpPromos">
        <a name="ntpPromos"></a>
        <h2>NTP Promos</h2>
        <div id="ntpPromoPreferences">
          <cr-expand-button
              ?expanded="${this.ntpPromoPreferencesExpanded_}"
              @expanded-changed="${this.onNtpPromoPreferencesExpandedChanged_}">
            <div id="label"><h3>NTP Promo Preferences</h3></div>
          </cr-expand-button>
          <div id="ntpPromoPrefData"
              ?hidden="${!this.ntpPromoPreferencesExpanded_}">
            ${this.ntpPromoPreferences_.map(item => html`
              <p><b>${item.name}</b> ${item.value}</p>`)}
            <p>
              Clicking the button below will reset all NTP promo preferences
              not tied to feature flags. These changes may not be reflected on
              NTP tabs that are already open.
            </p>
            <cr-button
                id="clearNtpPromoPreferences"
                @click="${this.onClearNtpPromoPreferencesClick_}">
              Clear All
            </cr-button>
          </div>
        </div>
        ${this.ntpPromos_.map(item => html`
          <user-education-internals-card
              id="${item.internalName}"
              ?hidden="${!this.promoFilter_(item)}"
              .promo="${item}"
              @clear-promo-data="${this.onNtpPromoClearPromoData_}">
          </user-education-internals-card>`)}
      </div>
      <div id="whatsNew">
        <h2>What's New</h2>
        <if expr="not is_chromeos">
          <div class="whats-new-section">
            <h3>Version Override</h3>
            <p>
              Providing a version override here will be used on the What's New
              page until reset by quitting the browser.
            </p>
            <cr-input type="number" id="whatsNewVersionOverride"
                label="What's New Version Override (current: ${
                    this.whatsNewVersionToRequest_})"
                min="${this.currentChromeVersion_ - 10}"
                max="${this.currentChromeVersion_ + 10}"
                error-message="Number must be within 10 of the current Chrome version (${
                    this.currentChromeVersion_})"
                .value="${this.whatsNewVersionToRequest_}">
              <cr-button slot="suffix"
                  @click="${this.onWhatsNewVersionOverrideClick_}">
                Set
              </cr-button>
            </cr-input>
            <div class="note">
              <p>
                Note that this will not reflect the experimental features of a
                different version or allow testing browser commands that do not
                exist in this version.
              </p>
            </div>
            <h3>Staging Environment</h3>
            <cr-button id="launch-whats-new-staging"
                @click="${this.onLaunchWhatsNewStagingClick_}">
              Launch staging
            </cr-button>
            <div class="note">
              <p>
                Note: This button will only request the staging page once.
                Closing the tab or opening another What's New page may request
                from the production environment again.
              </p>
              <p>
                Consider using the
                <span class="inline-code">--whats-new-use-staging</span>
                command-line switch instead. This switch will force the staging
                environment for the duration of the browser session.
              </p>
            </div>
            <h3>Debug Info</h3>
            <p>
              To view debug information for What's New, open the page and run
              <span class="inline-code">
                chromeWhatsNew.debugInfo()
              </span>
              in the javascript console.
            </p>
            <h3>Browser Commands</h3>
            <p>
              To test a browser command for What's New, open the page and run
              <span class="inline-code">
                chromeWhatsNew.triggerBrowserCommand(number)
              </span>
              in the javascript console.
            </p>
          </div>
        </if>
        ${this.whatsNewModules_.length > 0 ? html`
          <h3 class="whats-new-section">Modules</h3>` :
          ''}
        ${this.whatsNewModules_.map(item => html`
          <user-education-whats-new-internals-card
              id="${item.moduleName}"
              ?hidden="${!this.whatsNewFilter_(item)}"
              .item="${item}"
              type="module"
              @clear-whats-new-data="${this.onClearWhatsNewData_}">
          </user-education-whats-new-internals-card>`)}
        ${this.whatsNewEditions_.length > 0 ? html`
          <h3 class="whats-new-section">Editions</h3>` :
          ''}
        ${this.whatsNewEditions_.map(item => html`
          <user-education-whats-new-internals-card
              id="${item.editionName}"
              ?hidden="${!this.whatsNewFilter_(item)}"
              .item="${item}"
              type="edition"
              @clear-whats-new-data="${this.onClearWhatsNewData_}">
          </user-education-whats-new-internals-card>`)}
      </div>
      <div id="advanced">
        <a name="advanced"></a>
        <h2>Advanced</h2>
        <div id="session">
          <cr-expand-button
              ?expanded="${this.sessionExpanded_}"
              @expanded-changed="${this.onSessionExpandedChanged_}">
            <div id="label"><h3>Session, Grace Period, and Cooldown</h3></div>
          </cr-expand-button>
          <div id="sessionData" ?hidden="${!this.sessionExpanded_}">
            <p>
              Sessions are used to track when a user starts to use the browser
              after a period of inactivity.
            </p>
            <p>
              In addition to tracking user activity, several grace periods and
              cooldowns apply that may prevent IPH and "New" Badges from
              displaying:
            </p>
            <ul>
              <li>
                <strong>New profile grace period:</strong>
                All low-priority IPH and "New" Badges are blocked for several
                days on any new profile.
              </li>
              <li>
                <strong>Session start grace period:</strong>
                Heavyweight IPH are blocked for several minutes after the user
                starts interacting with Chrome after significant time away.
              </li>
              <li>
                <strong>Heavyweight IPH cooldown:</strong>
                Heavyweight IPH are blocked for several days after the user
                interacts with another heavyweight IPH.
              </li>
            </ul>
            <p>
              Session, grace period, and cooldown data:
            </p>
            ${this.sessionData_.map(item => html`
              <p><b>${item.name}</b> ${item.value}</p>`)}
            <p>
              Clicking the buttons below will modify the current session, last
              heavyweight promo, and/or profile creation times. The information
              above will be updated to show the current state of these values.
            </p>
            <cr-button id="forceNewSession"
                @click="${this.onForceNewSessionClick_}">
              Force New Session
            </cr-button>
            <cr-button id="removeGracePeriods"
                @click="${this.onRemoveGracePeriodsClick_}">
              Remove Grace Period
            </cr-button>
            <cr-button id="clearSession"
                @click="${this.onClearSessionDataClick_}">
              Clear Session Data
            </cr-button>
          </div>
        </div>
      </div>
    </div>
  </div>
  <div id="right" ?hidden="${this.narrow_}"></div>
</div>`;
  // clang-format on
}
