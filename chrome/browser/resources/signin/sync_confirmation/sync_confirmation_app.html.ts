// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SyncConfirmationAppElement} from './sync_confirmation_app.js';

export function getHtml(this: SyncConfirmationAppElement) {
  return html`<!--_html_template_start_-->

<!--
  Use the 'consent-description' attribute to annotate all the UI elements
  that are part of the text the user reads before consenting to the Sync
  data collection . Similarly, use 'consent-confirmation' on UI elements on
  which user clicks to indicate consent.
-->

<div class="main-container tangible-sync-style
    ${this.getMaybeDialogClass_()}">
  <img class="tangible-sync-style-dialog-illustration" alt=""
      ?hidden="${!this.isModalDialog_}">
  <img class="tangible-sync-style-left-banner" alt=""
      ?hidden="${this.isModalDialog_}">
  <img class="tangible-sync-style-right-banner" alt=""
      ?hidden="${this.isModalDialog_}">
  <div id="contentContainer" class="${this.getAnimationClass_()}">
    <div id="avatarContainer">
      <img class="loading-spinner"
          src="chrome://resources/images/throbber_small.svg"
          ?hidden="${!this.isPending_()}">
      <img id="avatar" alt="" src="${this.accountImageSrc_}"
          ?hidden="${this.isPending_()}">
      <div id="badge" class="work-badge"
          ?hidden="${this.shouldHideEnterpriseBadge_()}">
        <cr-icon icon="cr:domain" alt=""></cr-icon>
      </div>
    </div>
    <h1 class="title" consent-description>
      $i18n{syncConfirmationTitle}
    </h1>
    <div class="subtitle" consent-description>
      $i18n{syncConfirmationSyncInfoTitle}
    </div>
    <div id="syncBenefitsList">
      ${this.syncBenefitsList_.map(item => html`
        <div class="sync-benefit">
          <cr-icon class="sync-benefit-icon" icon="${item.iconName}"
              alt="">
          </cr-icon>
          <div class="sync-benefit-text" consent-description>
            ${this.i18n(item.title)}
          </div>
        </div>
      `)}
    </div>
    <div class="sync-info-desc secondary"
          ?hidden="${this.useClickableSyncInfoDesc_}" consent-description>
      $i18n{syncConfirmationSyncInfoDesc}
    </div>
      <localized-link class="sync-info-desc secondary"
          consent-description
          ?hidden="${!this.useClickableSyncInfoDesc_}"
          localized-string="$i18n{syncConfirmationSyncInfoDesc}"
          @link-clicked="${this.onDisclaimerClicked_}">
      </localized-link>
  </div>
</div>
<div class="action-row ${this.getAnimationClass_()}">
  ${this.anyButtonClicked_ ? html`<div class="spinner"></div>` : ''}
  <div class="action-container tangible-sync-style
      ${this.getMaybeDialogClass_()}">
    <cr-button id="confirmButton"
        class="${this.getConfirmButtonClass_()}"
        @click="${this.onConfirm_}"
        ?disabled="${this.anyButtonClicked_}" consent-confirmation
        ?autofocus="${this.isModalDialog_}">
      $i18n{syncConfirmationConfirmLabel}
    </cr-button>
    <if expr="is_macosx or is_linux or chromeos_ash or chromeos_lacros">
      <cr-button id="settingsButton" @click="${this.onGoToSettings_}"
          ?disabled="${this.anyButtonClicked_}" consent-confirmation>
        $i18n{syncConfirmationSettingsLabel}
      </cr-button>
    </if>
    <cr-button id="notNowButton"
        class="${this.getNotNowButtonClass_()}"
        @click="${this.onUndo_}" ?disabled="${this.anyButtonClicked_}">
      $i18n{syncConfirmationUndoLabel}
    </cr-button>
    <if expr="not (is_macosx or is_linux or chromeos_ash or chromeos_lacros)">
      <cr-button id="settingsButton" @click="${this.onGoToSettings_}"
          ?disabled="${this.anyButtonClicked_}" consent-confirmation>
        $i18n{syncConfirmationSettingsLabel}
      </cr-button>
    </if>
  </div>
</div>
<!--_html_template_end_-->`;
}
