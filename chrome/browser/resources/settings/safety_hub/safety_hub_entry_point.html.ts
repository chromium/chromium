// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsSafetyHubEntryPointElement} from './safety_hub_entry_point.js';

export function getHtml(this: SettingsSafetyHubEntryPointElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{safetyHub}">
  <settings-safety-hub-module class="cr-row first" id="module"
      header="${this.headerString_}" subheader="${this.subheaderString_}"
      header-icon-color="${this.computeHeaderIconColor_()}"
      header-icon="cr:security">
    <cr-button id="button" @click="${this.onClick_}" slot="button-container"
        class="${this.computeButtonClass_()}">
      $i18n{safetyHubEntryPointButtonLabel}
    </cr-button>
  </settings-safety-hub-module>
</settings-section>
<!--_html_template_end_-->`;
}
