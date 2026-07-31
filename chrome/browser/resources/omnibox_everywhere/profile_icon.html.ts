// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereProfileIconElement} from './profile_icon.js';

export function getHtml(this: OmniboxEverywhereProfileIconElement) {
  return html`
    <img id="profileIcon" src="${this.profileAvatarUrl_}"
        alt="${this.i18n('profileButtonLabel')}"
        class="${this.profilePickerEnabled_ ? 'clickable' : ''}"
        @click="${this.onProfileIconClick_}">
    <cr-action-menu id="profileMenu">
      <div class="profile-card">
        <div class="profile-card-header">
          <img class="profile-card-avatar" src="${this.profileAvatarUrl_}" alt="">
          <div class="profile-card-info">
            <div class="profile-card-name">${this.profileName_}</div>
            ${this.profileEmail_ ? html`
              <div class="profile-card-email">${this.profileEmail_}</div>
            ` : ''}
          </div>
        </div>
        <div class="profile-card-divider"></div>
        <button class="dropdown-item profile-card-switch-button" @click="${this.onSwitchProfileClick_}">
          <div class="profile-card-switch-icon" aria-hidden="true"></div>
          <span class="profile-card-switch-label">Switch Profile</span>
        </button>
      </div>
    </cr-action-menu>
  `;
}
