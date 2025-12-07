// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SyncDisabledConfirmationAppElement} from './sync_disabled_confirmation_app.js';

export function getHtml(this: SyncDisabledConfirmationAppElement) {
  return html`<!--_html_template_start_-->
<!--
  Use the 'consent-description' attribute to annotate all the UI elements
  that are part of the text the user reads before consenting to the Sync
  data collection . Similarly, use 'consent-confirmation' on UI elements on
  which user clicks to indicate consent.
-->
<div class="container">
  <div class="top-title-bar" consent-description>
    $i18n{syncDisabledConfirmationTitle}
  </div>
  <div class="details" id="syncDisabledDetails">
    <div class="body text" consent-description>
      $i18n{syncDisabledConfirmationDetails}
    </div>
  </div>
  <div class="action-container">
    <cr-button class="action-button" id="confirmButton"
        consent-confirmation @click="${this.onConfirm_}">
      $i18n{syncDisabledConfirmationConfirmLabel}
    </cr-button>
    <cr-button id="undoButton" @click="${this.onUndo_}"
        ?hidden="${this.signoutDisallowed_}">
      $i18n{syncDisabledConfirmationUndoLabel}
    </cr-button>
  </div>
</div>
<!--_html_template_end_-->`;
}
