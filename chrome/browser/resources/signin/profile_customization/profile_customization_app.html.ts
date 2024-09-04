// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {ProfileCustomizationAppElement} from './profile_customization_app.js';

export function getHtml(this: ProfileCustomizationAppElement) {
  return html`<!--_html_template_start_-->
<cr-view-manager role="dialog" id="viewManager" class="in-dialog-design"
    aria-labelledby="title" aria-describedby="content">
  <div id="customizeDialog" slot="view" class="active">
    <!-- An extra wrapper is needed because cr-view-manager incorrectly
         displays views with display: flex -->
    <div class="profile-customization-wrapper">
      <div id="header">
        <div id="avatarContainer">
          <img id="avatar" alt="" src="${this.pictureUrl_}">
          ${!this.isLocalProfileCreation_ ? html`
            <div class="avatar-badge" id="workBadge"
                ?hidden="${!this.isManaged_}">
              <cr-icon class="icon" icon="cr:domain"></cr-icon>
            </div>
          ` : ''}
          ${this.isLocalProfileCreation_ ? html`
            <div class="avatar-badge">
              <cr-icon-button id="customizeAvatarIcon" iron-icon="cr:create"
                  @click="${this.onCustomizeAvatarClick_}"
                  title="$i18n{profileCustomizationCustomizeAvatarLabel}"
                  aria-label="$i18n{profileCustomizationCustomizeAvatarLabel}">
              </cr-icon-button>
            </div>
          ` : ''}
        </div>
      </div>

      <div id="body">
        <div id="title">${this.welcomeTitle_}</div>
      </div>

      <cr-input id="nameInput" pattern=".*\\S.*" .value="${this.profileName_}"
          @value-changed="${this.onProfileNameChanged_}"
          aria-label="$i18n{profileCustomizationInputLabel}" auto-validate
          placeholder="$i18n{profileCustomizationInputPlaceholder}" autofocus
          error-message="$i18n{profileCustomizationInputErrorMessage}"
          required spellcheck="false" @blur="${this.validateInputOnBlur_}">
      </cr-input>

      <div id="pickThemeContainer">
        <cr-theme-color-picker columns="6"></cr-theme-color-picker>
      </div>

      <div class="action-container">
        <cr-button id="doneButton" class="action-button"
            ?disabled="${this.isDoneButtonDisabled_()}"
            @click="${this.onDoneCustomizationClicked_}">
          $i18n{profileCustomizationDoneLabel}
        </cr-button>
        ${this.shouldShowCancelButton_() ? html`
          <cr-button id="skipButton"
              @click="${this.onSkipCustomizationClicked_}">
            $i18n{profileCustomizationSkipLabel}
          </cr-button>
        ` : ''}
        ${this.isLocalProfileCreation_ ? html`
          <cr-button id="deleteProfileButton"
              @click="${this.onDeleteProfileClicked_}">
            $i18n{profileCustomizationDeleteProfileLabel}
          </cr-button>
        ` : ''}
      </div>
    </div>
  </div>

  ${this.isLocalProfileCreation_ ? html`
    <div id="selectAvatarDialog" slot="view">
      <div class="select-avatar-header">
        <cr-icon-button iron-icon="cr:arrow-back" id="selectAvatarBackButton"
            title="$i18n{profileCustomizationAvatarSelectionBackButtonLabel}"
            aria-label="$i18n{profileCustomizationAvatarSelectionBackButtonLabel}"
            @click="${this.onSelectAvatarBackClicked_}">
        </cr-icon-button>
        <div class="select-avatar-title">
          $i18n{profileCustomizationAvatarSelectionTitle}
        </div>
      </div>
      <div id="selectAvatarWrapper" class="custom-scrollbar">
        <cr-profile-avatar-selector .avatars="${this.availableIcons_}"
            .selectedAvatar="${this.selectedAvatar_}"
            @selected-avatar-changed="${this.onSelectedAvatarChanged_}"
            columns="5">
        </cr-profile-avatar-selector>
      </div>
      <div class="action-container">
        <cr-button id="selectAvatarConfirmButton" class="action-button"
            @click="${this.onSelectAvatarConfirmClicked_}">
          $i18n{profileCustomizationDoneLabel}
        </cr-button>
      </div>
    </div>
  ` : ''}

</cr-view-manager>
<!--_html_template_end_-->`;
}
