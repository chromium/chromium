// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {isGlicVersion} from './profile_picker_flags.js';
import type {ProfilePickerMainViewElement} from './profile_picker_main_view.js';

export function getHtml(this: ProfilePickerMainViewElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${isGlicVersion() ? html`
  <link href="glic_profile_branding.css" rel="stylesheet" />
` : ''}
<div class="flex-container">
  <div class="title-container">
    <img id="pickerLogo" @click="${this.onProductLogoClick_}"
        src="picker_logo.svg" role="presentation">
    <h1 class="title" .innerHTML="${this.getTitle_()}"></h1>
    <div class="subtitle" .innerHTML="${this.getSubtitle_()}"></div>
  </div>
  <div id="profilesWrapper" ?hidden="${(this.shouldHideProfilesWrapper_())}">
    <div id="profilesContainer" class="custom-scrollbar">
      ${this.profilesList_.map((item, index) => html`
        <profile-card class="profile-item" data-index="${index}"
            .profileState="${item}" .disabled="${this.pickerButtonsDisabled_}"
            @toggle-drag="${this.onToggleDrag_}"
            @disable-all-picker-buttons="${this.onDisableAllPickerButtons_}">
        </profile-card>
      `)}
      <cr-button id="addProfile" class="profile-item"
          @click="${this.onAddProfileClick_}"
          ?hidden="${!this.profileCreationAllowed_}"
          ?disabled="${this.pickerButtonsDisabled_}"
          aria-labelledby="addProfileButtonLabel">
        <div id="addProfileButtonLabel"
            class="profile-card-info prominent-text">
          $i18n{addSpaceButton}
        </div>
        <cr-icon icon="profiles:add"></cr-icon>
      </cr-button>
    </div>
  </div>
  <div id="footer-text" class="subtitle"
      ?hidden="${this.shouldHideFooterText_()}">
    $i18nRaw{glicAddProfileHelper}
  </div>
</div>
<div class="footer">
  <div class="footer-buttons-container">
    <cr-button id="browseAsGuestButton"
        @click="${this.onLaunchGuestProfileClick_}"
        ?hidden="${!this.guestModeEnabled_}"
        ?disabled="${this.pickerButtonsDisabled_}">
      <cr-icon icon="profiles:account-box" slot="prefix-icon"></cr-icon>
      $i18n{browseAsGuestButton}
    </cr-button>
    <cr-button id="openAllProfilesButton"
        class="action-button"
        @click="${this.onOpenAllProfilesClick_}"
        ?hidden="${!this.shouldShowOpenAllProfilesButton_}"
        ?disabled="${this.pickerButtonsDisabled_}">
      $i18n{openAllProfilesButtonText}
    </cr-button>
  </div>

  ${this.isRefreshedUI_ ? html`
    <div id="ask-on-startup-container" ?hidden="${this.hideAskOnStartup_}">
      <span id="ask-on-startup-label" aria-hidden="true">
        $i18n{askOnStartupText}
      </span>
      <cr-toggle id="askOnStartup"
          aria-labelledby="ask-on-startup-label"
          ?checked="${this.askOnStartup_}"
          @checked-changed="${this.onAskOnStartupCheckedChanged_}">
      </cr-toggle>
    </div>
  ` : html`
    <cr-checkbox id="askOnStartup" ?checked="${this.askOnStartup_}"
        @checked-changed="${this.onAskOnStartupCheckedChanged_}"
        ?hidden="${this.hideAskOnStartup_}">
      $i18n{askOnStartupText}
    </cr-checkbox>
  `}
</div>

<signin-error-dialog id="signinErrorDialog">
</signin-error-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
