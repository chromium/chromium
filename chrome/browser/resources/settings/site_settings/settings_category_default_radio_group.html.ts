// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ContentSetting} from './constants.js';
import type {SettingsCategoryDefaultRadioGroupElement} from './settings_category_default_radio_group.js';

export function getHtml(this: SettingsCategoryDefaultRadioGroupElement) {
  return html`<!--_html_template_start_-->
<div id="radioSection">
  <h2>${this.header}</h2>
  <div id="radioGroupSubLabel" class="secondary">
    ${this.description}
  </div>
  <settings-radio-group
      id="settingsCategoryDefaultRadioGroup"
      pref-key="${this.prefKey_}"
      selectable-elements="settings-collapse-radio-button"
      @change="${this.onSelectedRadioChange_}"
      group-aria-label="${this.header}">
    <settings-collapse-radio-button
        id="allowRadioOption"
        class="${this.getButtonClass_(this.allowOptionSubLabel)}"
        name="${ContentSetting.ALLOW}"
        pref-key="${this.prefKey_}"
        label="${this.allowOptionLabel}"
        sub-label="${this.allowOptionSubLabel}"
        ?disabled="${this.isRadioGroupDisabled_()}"
        icon="${this.allowOptionIcon}"
        ?hidden="${!this.allowOptionLabel}"
        no-collapse>
    </settings-collapse-radio-button>
    <settings-collapse-radio-button
        id="askRadioOption"
        class="${this.getButtonClass_(this.askOptionSubLabel)}"
        name="${ContentSetting.ASK}"
        pref-key="${this.prefKey_}"
        label="${this.askOptionLabel}"
        sub-label="${this.askOptionSubLabel}"
        ?disabled="${this.isRadioGroupDisabled_()}"
        icon="${this.askOptionIcon}"
        ?hidden="${!this.askOptionLabel}"
        no-collapse>
    </settings-collapse-radio-button>
    <settings-collapse-radio-button
        id="blockRadioOption"
        class="${this.getButtonClass_(this.blockOptionSubLabel)}"
        name="${ContentSetting.BLOCK}"
        pref-key="${this.prefKey_}"
        label="${this.blockOptionLabel}"
        sub-label="${this.blockOptionSubLabel}"
        ?disabled="${this.isRadioGroupDisabled_()}"
        icon="${this.blockOptionIcon}"
        ?hidden="${!this.blockOptionLabel}"
        no-collapse>
    </settings-collapse-radio-button>
  </settings-radio-group>
</div>
<!--_html_template_end_-->`;
}
