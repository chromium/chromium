// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {SettingsBatteryPageElement} from './battery_page.js';
import {BatterySaverModeState} from './performance_metrics_proxy.js';

export function getHtml(this: SettingsBatteryPageElement) {
  return html`<!--_html_template_start_-->
<settings-section
    ?show-send-feedback-button="${this.showSendFeedbackButton_()}"
    @send-feedback="${this.onSendFeedback_}"
    page-title="$i18n{batteryPageTitle}">
  <if expr="is_chromeos">
  ${this.isBatterySaverModeManagedByOs_ ? html`
    <cr-link-row id="batterySaverOSSettingsLinkRow"
        label="$i18n{batterySaverModeLabel}"
        sub-label="$i18n{batterySaverModeLinkOsDescription}"
        @click="${this.onOsPowerSettingsClick_}"
        external>
    </cr-link-row>
  ` : ''}
  </if>
  ${!this.isBatterySaverModeManagedByOs_ ? html`
    <settings-toggle-button id="toggleButton" @change="${this.onChange_}"
        pref-key="performance_tuning.battery_saver_mode.state"
        label="$i18n{batterySaverModeLabel}"
        sub-label-with-link="$i18n{batterySaverModeDescription}"
        @sub-label-link-clicked="${this.onBatterySaverSubLabelLinkClicked_}"
        .numericUncheckedValues="${this.numericUncheckedValues_}"
        .numericCheckedValue="${BatterySaverModeState.ENABLED_BELOW_THRESHOLD}">
    </settings-toggle-button>
    <cr-collapse id="radioGroupCollapse"
        ?opened="${this.isBatterySaverModeEnabled_()}">
      <div class="cr-row continuation battery-saver-radio-group">
        <settings-radio-group id="radioGroup" @change="${this.onChange_}"
            pref-key="performance_tuning.battery_saver_mode.state"
            group-aria-label="$i18n{batterySaverModeRadioGroupAriaLabel}">
          <controlled-radio-button
              label="$i18n{batterySaverModeEnabledBelowThresholdLabel}"
              name="${BatterySaverModeState.ENABLED_BELOW_THRESHOLD}"
              pref-key="performance_tuning.battery_saver_mode.state">
          </controlled-radio-button>
          <controlled-radio-button id="enabledOnBatteryButton"
              label="$i18n{batterySaverModeEnabledOnBatteryLabel}"
              name="${BatterySaverModeState.ENABLED_ON_BATTERY}"
              pref-key="performance_tuning.battery_saver_mode.state">
          </controlled-radio-button>
        </settings-radio-group>
      </div>
    </cr-collapse>
  ` : ''}
</settings-section><!--_html_template_end_-->`;
}
