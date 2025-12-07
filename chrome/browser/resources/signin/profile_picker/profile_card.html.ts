// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProfileCardElement} from './profile_card.js';
import {isGlicVersion} from './profile_picker_flags.js';

export function getHtml(this: ProfileCardElement) {
  return html`<!--_html_template_start_-->
<div id="profileCardContainer">
  <cr-button id="profileCardButton" @click="${this.onProfileClick_}"
      ?disabled="${this.disabled}"
      aria-label="${this.profileState.profileCardButtonLabel}">
    <div id="avatarContainer">
      <img class="profile-avatar" alt="" .src="${this.profileState.avatarIcon}">
      <div id="iconContainer"
          ?hidden="${!this.profileState.avatarBadge.length}">
        <cr-icon icon="${this.profileState.avatarBadge}"></cr-icon>
      </div>
    </div>
    <div id="gaiaName" class="profile-card-info secondary-text"
        ?hidden="${this.profileState.needsSignin}">
      ${this.profileState.gaiaName}
    </div>
    <div id="forceSigninContainer" class="profile-card-info secondary-text"
        ?hidden="${!this.profileState.needsSignin}">
      <div>$i18n{needsSigninPrompt}</div>
      <cr-icon id="forceSigninIcon" icon="profiles:lock"></cr-icon>
    </div>
  </cr-button>
  <div id="profileNameInputWrapper">
    <cr-input class="profile-card-info prominent-text" id="nameInput"
        aria-label="$i18n{profileCardInputLabel}"
        .value="${this.profileState.localProfileName}"
        @change="${this.onProfileNameChanged_}"
        @keydown="${this.onProfileNameKeydown_}"
        @blur="${this.onProfileNameInputBlur_}" pattern="${this.pattern_}"
        auto-validate spellcheck="false"
        @pointerenter="${this.onNameInputPointerEnter_}"
        @pointerleave="${this.onNameInputPointerLeave_}"
        ?disabled="${
      isGlicVersion() || this.profileState.hasEnterpriseLabel}" required>
    </cr-input>
    <div id="hoverUnderline" ?hidden="${
      isGlicVersion() || this.profileState.hasEnterpriseLabel}"></div>
  </div>
  <profile-card-menu .profileState="${this.profileState}"
      ?hidden="${isGlicVersion()}">
  </profile-card-menu>
</div>
<cr-tooltip id="gaiaNameTooltip" for="gaiaName" manual-mode offset="0"
    position="bottom" aria-hidden="true">
  ${this.profileState.gaiaName}
</cr-tooltip>
<cr-tooltip id="tooltip" for="nameInput" manual-mode offset="-10"
    aria-hidden="true">
  ${this.getNameInputTooltipText()}
</cr-tooltip>
<!--_html_template_end_-->`;
}
