// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProfileSwitchElement} from './profile_switch.js';

export function getHtml(this: ProfileSwitchElement) {
  return html`<!--_html_template_start_-->
<div id="outerContainer">
  <div id="profileCardContainer">
    <div id="avatarContainer">
      <img class="profile-avatar" alt="" src="${this.profileState_.avatarIcon}">
      <div id="iconContainer"
          ?hidden="${!this.profileState_.avatarBadge.length}">
        <cr-icon icon="${this.profileState_.avatarBadge}"></cr-icon>
      </div>
    </div>
    <div id="profileName" class="profile-card-info prominent-text">
      ${this.profileState_.localProfileName}
    </div>
    <div id="gaiaName" class="profile-card-info secondary-text">
      ${this.profileState_.gaiaName}
    </div>
  </div>
  <div id="titleContainer">
    <h1 class="title">$i18n{profileSwitchTitle}</h1>
    <div class="subtitle">$i18n{profileSwitchSubtitle}</div>
  </div>
  <div id="actionContainer">
    <cr-button id="cancelButton" @click="${this.onCancelClick_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="switchButton" class="action-button"
        ?disabled="${!this.isProfileStateInitialized_}"
        @click="${this.onSwitchClick_}">
      $i18n{switchButtonLabel}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
}
