// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProfileCardMenuElement} from './profile_card_menu.js';

export function getHtml(this: ProfileCardMenuElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button iron-icon="cr:more-vert" id="moreActionsButton"
    @click="${this.onMoreActionsButtonClicked_}" title="$i18n{profileMenuName}"
    aria-label="${this.moreActionsButtonAriaLabel_}">
</cr-icon-button>

<cr-action-menu id="actionMenu" role-description="$i18n{menu}">
  <button class="dropdown-item" @click="${this.onCustomizeButtonClicked_}">
    <cr-icon icon="cr:create" aria-hidden="true"></cr-icon>
    $i18n{profileMenuCustomizeText}
  </button>
  <button class="dropdown-item" @click="${this.onRemoveButtonClicked_}">
    <cr-icon icon="cr:delete" aria-hidden="true"></cr-icon>
    $i18n{profileMenuRemoveText}
  </button>
</cr-action-menu>

<cr-dialog id="removeConfirmationDialog" ignore-enter-key>
  <div slot="title">${this.removeWarningTitle_}</div>
  <if expr="chromeos_lacros">
    <div id="removeWarningHeader" slot="header" class="warning-message"
        .innerHTML="${this.getRemoveWarningTextForLacros_()}">
    </div>
  </if>
  <if expr="not chromeos_lacros">
    <div id="removeWarningHeader" slot="header" class="warning-message">
      ${this.removeWarningText_}
      <span id="userName" ?hidden="${!this.profileState.isSyncing}"
          class="key-text">
        ${this.profileState.userName}
      </span>
    </div>
  </if>
  <div slot="body">
    <div id="removeActionDialogBody">
      <div id="profileCardContainer">
        <div id="avatarContainer">
          <img class="profile-avatar" alt=""
              src="${this.profileState.avatarIcon}">
        </div>
        <div id="profileName" class="profile-card-info prominent-text">
          ${this.profileState.localProfileName}
        </div>
        <div id="gaiaName" class="profile-card-info secondary-text">
          ${this.profileState.gaiaName}
        </div>
      </div>
      <table class="statistics">
        ${this.profileStatistics_.map(item => html`
          <tr>
            <td class="category">${this.getProfileStatisticText_(item)}</td>
            <td class="count">${this.getProfileStatisticCount_(item)}</td>
          </tr>
        `)}
      </table>
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onRemoveCancelClicked_}">
      $i18n{cancel}
    </cr-button>
    <cr-button id="removeConfirmationButton" class="action-button"
        @click="${this.onRemoveConfirmationClicked_}">
      $i18n{profileMenuRemoveText}
    </cr-button>
  </div>
</cr-dialog>

<if expr="chromeos_lacros">
  <cr-dialog id="removePrimaryLacrosProfileDialog">
    <div slot="title" class="key-text">
      $i18n{lacrosPrimaryProfileDeletionWarningTitle}
    </div>
    <div slot="body" class="warning-message">
      ${this.removePrimaryLacrosProfileWarning_}
    </div>
    <div slot="button-container">
      <cr-button class="action-button"
          @click="${this.onRemovePrimaryLacrosProfileCancelClicked_}">
        $i18n{lacrosPrimaryProfileDeletionWarningConfirmation}
      </cr-button>
    </div>
  </cr-dialog>
</if>
<!--_html_template_end_-->`;
}
