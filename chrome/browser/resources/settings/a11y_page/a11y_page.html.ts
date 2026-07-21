// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsA11yPageElement} from './a11y_page.js';
import {ToastAlertLevel} from './a11y_page.js';

export function getHtml(this: SettingsA11yPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <settings-section page-title="$i18n{a11yPageTitle}">
      <div route-path="default">
<if expr="is_chromeos">
        <cr-link-row
            label="$i18n{manageAccessibilityFeatures}"
            @click="${this.onManageSystemAccessibilityFeaturesClick_}"
            sub-label="$i18n{moreFeaturesLinkDescription}" external>
        </cr-link-row>
</if>

<if expr="is_macosx or is_win">
        ${this.enableLiveCaption_ ? html`
          <settings-live-caption></settings-live-caption>
        ` : ''}
        <cr-link-row id="captions"
            class="hr"
            label="$i18n{captionsPreferencesTitle}"
            sub-label="$i18n{captionsPreferencesSubtitle}"
            @click="${this.onCaptionsClick_}"
            button-aria-description="$i18n{opensInNewTab}"
            external>
        </cr-link-row>
</if>

<if expr="is_linux">
          <cr-link-row id="captions"
              class="hr"
              label="$i18n{captionsTitle}"
              @click="${this.onCaptionsClick_}"
              role-description="$i18n{subpageArrowRoleDescription}">
          </cr-link-row>
</if>

<if expr="not is_chromeos">
        <settings-toggle-button
            class="hr"
            pref-key="settings.a11y.focus_highlight"
            @setting-boolean-control-change="${
                this.onFocusHighlightSettingBooleanControlChange_}"
            label="$i18n{focusHighlightLabel}">
        </settings-toggle-button>
        <settings-toggle-button
            class="hr"
            pref-key="settings.a11y.caretbrowsing.enabled"
            @change="${this.onA11yCaretBrowsingChange_}"
            label="$i18n{caretBrowsingTitle}"
            sub-label="$i18n{caretBrowsingSubtitle}">
        </settings-toggle-button>
</if>
        <settings-toggle-button
            class="hr"
            ?hidden="${!this.hasScreenReader_}"
            pref-key="settings.a11y.enable_accessibility_image_labels"
            @change="${this.onA11yImageLabelsChange_}"
            label="$i18n{accessibleImageLabelsTitle}"
            sub-label="$i18n{accessibleImageLabelsSubtitle}">
        </settings-toggle-button>
        ${this.showAxTreeFixingSection_ ? html`
          <settings-toggle-button id="axTreeFixing"
              class="hr"
              pref-key="settings.a11y.enable_ax_tree_fixing"
              label="$i18n{axTreeFixingTitle}"
              sub-label="$i18n{axTreeFixingSubtitle}">
          </settings-toggle-button>
        ` : ''}
<if expr="is_win or is_linux or is_macosx">
        ${this.showAxAnnotationsSection_ ? html`
          <settings-ax-annotations-section
              id="AxAnnotationsSection">
          </settings-ax-annotations-section>
        ` : ''}
</if>
<if expr="is_win or is_linux">
        <settings-toggle-button
            class="hr"
            pref-key="settings.a11y.overscroll_history_navigation"
            @change="${this.onOverscrollHistoryNavigationChange_}"
            label="$i18n{overscrollHistoryNavigationTitle}"
            sub-label="$i18n{overscrollHistoryNavigationSubtitle}">
        </settings-toggle-button>
</if>
<if expr="is_macosx">
        <cr-link-row
            class="hr"
            @click="${this.onMacTrackpadGesturesLinkClick_}"
            button-aria-description="$i18n{opensInNewTab}"
            label="$i18n{overscrollHistoryNavigationTitle}"
            sub-label="$i18n{overscrollHistoryNavigationSubtitle}"
            external>
        </cr-link-row>
</if>
<if expr="not is_chromeos">
        <settings-toggle-button class="hr" id="toastToggle"
            pref-key="settings.toast.alert_level"
            .numericUncheckedValues="${this.numericUncheckedToastAlertValues_}"
            .numericCheckedValue="${ToastAlertLevel.ALL}"
            @change="${this.onToastAlertLevelChange_}"
            label="$i18n{toastAlertLevelTitle}"
            sub-label="$i18n{toastAlertLevelDescription}">
        </settings-toggle-button>
</if>
        <cr-link-row class="hr" label="$i18n{moreFeaturesLink}"
            @click="${this.onMoreFeaturesLinkClick_}"
            sub-label="$i18n{a11yWebStore}"
            button-aria-description="$i18n{opensInNewTab}"
            external>
        </cr-link-row>
      </div>
    </settings-section>
<!--_html_template_end_-->`;
  // clang-format on
}
