// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsOnStartupPageElement} from './on_startup_page.js';
import {PrefValues} from './on_startup_page.js';

export function getHtml(this: SettingsOnStartupPageElement) {
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{onStartup}"
    class="cr-centered-card-container">
  <div class="cr-row first">
    <settings-radio-group id="onStartupRadioGroup"
        class="flex"
        pref-key="session.restore_on_startup"
        group-aria-label="$i18n{onStartup}">
      <controlled-radio-button name="${PrefValues.OPEN_NEW_TAB}"
          pref-key="session.restore_on_startup"
          label="$i18n{onStartupOpenNewTab}"
          no-extension-indicator>
      </controlled-radio-button>
      ${this.ntpExtension_ ? html`
        <extension-controlled-indicator
            .extensionId="${this.ntpExtension_.id}"
            .extensionName="${this.ntpExtension_.name}"
            .extensionCanBeDisabled="${this.ntpExtension_.canBeDisabled}">
        </extension-controlled-indicator>
      ` : ''}
      <controlled-radio-button name="${PrefValues.CONTINUE}"
          pref-key="session.restore_on_startup"
          label="$i18n{onStartupContinue}">
      </controlled-radio-button>
      <controlled-radio-button name="${PrefValues.OPEN_SPECIFIC}"
          pref-key="session.restore_on_startup"
          label="$i18n{onStartupOpenSpecific}">
      </controlled-radio-button>
      <controlled-radio-button
          name="${PrefValues.CONTINUE_AND_OPEN_SPECIFIC}"
          pref-key="session.restore_on_startup"
          label="$i18n{onStartupContinueAndOpenSpecific}"
          ?hidden="${!this.showContinueAndOpenSpecific_()}">
      </controlled-radio-button>
    </settings-radio-group>
  </div>
  ${this.showStartupUrls_() ? html`
    <settings-startup-urls-page></settings-startup-urls-page>
  ` : ''}
<if expr="is_win">
  ${this.isForegroundLaunchFeatureEnabled_ ? html`
    <settings-toggle-button class="hr" id="foregroundLaunchOnStartup"
        pref-key="launch_on_login.foreground.enabled"
        label="$i18n{onStartupForegroundLaunchOnStartupLabel}"
        sub-label="$i18n{onStartupForegroundLaunchOnStartupSubLabel}">
    </settings-toggle-button>
  ` : ''}
</if>
</settings-section>
<!--_html_template_end_-->`;
}
