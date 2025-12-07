// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SigninEmailConfirmationAppElement} from './signin_email_confirmation_app.js';

export function getHtml(this: SigninEmailConfirmationAppElement) {
  return html`<!--_html_template_start_-->
<div class="container">
  <div class="top-title-bar" id='dialogTitle'>
  </div>
  <div class="details">
    <cr-radio-group selected="createNewUser">
      <cr-radio-button id="createNewUserRadioButton"
          name="createNewUser">
        <div class="radio-button-title-container">
          $i18n{signinEmailConfirmationCreateProfileButtonTitle}
        </div>
        <div class="radio-button-subtitle-container"
            id="createNewUserRadioButtonSubtitle">
        </div>
      </cr-radio-button>
      <cr-radio-button id="startSyncRadioButton" name="startSync">
        <div class="radio-button-title-container">
          $i18n{signinEmailConfirmationStartSyncButtonTitle}
        </div>
        <div class="radio-button-subtitle-container"
            id="startSyncRadioButtonSubtitle">
        </div>
      </cr-radio-button>
    </cr-radio-group>
  </div>
  <div class="action-container">
    <cr-button class="action-button" id="confirmButton"
        @click="${this.onConfirm_}" autofocus>
      $i18n{signinEmailConfirmationConfirmLabel}
    </cr-button>
    <cr-button id="closeButton" @click="${this.onCancel_}">
      $i18n{signinEmailConfirmationCloseLabel}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
}
