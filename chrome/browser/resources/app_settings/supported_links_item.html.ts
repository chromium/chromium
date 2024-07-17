// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SupportedLinksItemElement} from './supported_links_item.js';

export function getHtml(this: SupportedLinksItemElement) {
  return html`<!--_html_template_start_-->
<div class="permission-section-header">
  <localized-link id="heading" class="header-text"
      @link-clicked="${this.launchDialog_}"
      .localizedString=
          "${this.i18nAdvanced('appManagementIntentSettingsTitle')}">
  </localized-link>
</div>
${this.disabled_ ? html`
  <span class="info-text-row" id="disabledExplanationText">
    <cr-icon id="infoIcon" icon="app-management:info"></cr-icon>
    <localized-link id="infoString"
        .localizedString="${this.getDisabledExplanation_()}">
    </localized-link>
  </span>
` : ''}
<div class="list-frame">
  <cr-radio-group id="radioGroup"
      .selected="${this.getCurrentPreferredApp_()}"
      @selected-changed="${this.onSupportedLinkPrefChanged_}"
      ?disabled="${this.disabled_}">
    <cr-radio-button
        id="preferredRadioButton"
        name="preferred"
        label="${this.getPreferredLabel_()}">
    </cr-radio-button>
    <cr-radio-button
        id="browserRadioButton"
        name="browser"
        label="$i18n{appManagementIntentSharingOpenBrowserLabel}">
    </cr-radio-button>
    ${this.showOverlappingAppsWarning_ ? html`
      <div id="overlapWarning">${this.overlappingAppsWarning_}</div>
    ` : ''}
  </cr-radio-group>
</div>
${this.showSupportedLinksDialog_ ? html`
  <app-management-supported-links-dialog id="dialog" .app="${this.app}"
      @close="${this.onDialogClose_}">
  </app-management-supported-links-dialog>
` : ''}
${this.showOverlappingAppsDialog_ ? html`
  <app-management-supported-links-overlapping-apps-dialog
      id="overlapDialog"
      .app="${this.app}"
      .apps="${this.apps}"
      @close="${this.onOverlappingDialogClosed_}"
      .overlappingAppIds="${this.overlappingAppIds_}">
  </app-management-supported-links-overlapping-apps-dialog>
` : ''}
<!--_html_template_end_-->`;
}
