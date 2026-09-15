// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsDoNotTrackToggleElement} from './do_not_track_toggle.js';

export function getHtml(this: SettingsDoNotTrackToggleElement) {
  return html`<!--_html_template_start_-->
<settings-toggle-button id="toggle" class="toggle"
    label="$i18n{doNotTrack}"
    pref-key="enable_do_not_track"
    @settings-boolean-control-change="${this.onSettingsBooleanControlChange_}"
    sub-label="${this.doNotTrackSublabel_}"
    icon="settings:forward"
    no-set-pref>
</settings-toggle-button>
${this.showDialog_ ? html`
  <cr-dialog id="confirmDialog"
      close-text="$i18n{close}" @cancel="${this.onDialogCancel_}"
      @close="${this.onDialogClose_}" show-on-attach>
    <div slot="title">$i18n{doNotTrackDialogTitle}</div>
    <div slot="body">$i18n{doNotTrackDialogMessage}
      <a href="$i18nRaw{doNotTrackLearnMoreURL}" target="_blank"
          aria-description="$i18n{opensInNewTab}"
          aria-label="$i18n{doNotTrackDialogLearnMoreA11yLabel}">
        $i18n{learnMore}
      </a>
    </div>
    <div slot="button-container">
      <cr-button class="cancel-button" @click="${this.onCancelClick_}">
        $i18n{cancel}
      </cr-button>
      <cr-button class="action-button" @click="${this.onConfirmClick_}">
        $i18n{confirm}
      </cr-button>
    </div>
  </cr-dialog>` : ''}
<!--_html_template_end_-->`;
}
