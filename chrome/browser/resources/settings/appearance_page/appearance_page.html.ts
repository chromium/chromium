// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsAppearancePageElement} from './appearance_page.js';

export function getHtml(this: SettingsAppearancePageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{appearancePageTitle}">
  <div route-path="default">
    <div class="settings-row first" id="themeRow"
        ?hidden="${!this.pageVisibility_.setTheme}">
      <cr-link-row id="openTheme" class="first"
          ?hidden="${!this.pageVisibility_.setTheme}"
          label="$i18n{themes}" sub-label="${this.themeSublabel_}"
          @click="${this.onThemeClick_}" external></cr-link-row>
<if expr="not is_linux">
      ${this.themeIdPref_?.value ? html`
        <div class="separator"></div>
        <cr-button id="useDefault" @click="${this.onUseDefaultClick_}"
            aria-label="$i18n{resetToDefaultTheme}">
          $i18n{resetToDefault}
        </cr-button>
      ` : ''}
</if>
<if expr="is_linux">
      <div class="settings-row continuation"
          ?hidden="${!this.showThemesSecondary_()}"
          id="themesSecondaryActions">
        <div class="separator"></div>
        ${this.showUseClassic_() ? html`
          <cr-button id="useDefault" @click="${this.onUseDefaultClick_}">
            $i18n{useClassicTheme}
          </cr-button>
        ` : ''}
        ${this.showUseGtk_() ? html`
          <cr-button id="useGtk" @click="${this.onUseGtkClick_}">
            $i18n{useGtkTheme}
          </cr-button>
        ` : ''}
        ${this.showUseQt_() ? html`
          <cr-button id="useQt" @click="${this.onUseQtClick_}">
            $i18n{useQtTheme}
          </cr-button>
        ` : ''}
      </div>
</if>
    </div>
    <div id="toolbarRow" class="settings-row">
      <cr-link-row id="customizeToolbar"
          label="$i18n{customizeToolbar}"
          @click="${this.onCustomizeToolbarClick_}" external>
      </cr-link-row>
      ${this.showResetPinnedActionsButton_ ? html`
        <div class="separator"></div>
        <cr-button id="resetPinnedToolbarActions"
            @click="${this.onResetPinnedToolbarActionsClick_}"
            aria-label="$i18n{resetToolbarToDefault}">
          $i18n{resetToDefault}
        </cr-button>
      ` : ''}
    </div>
    <div id="colorSchemeModeRow" class="cr-row"
        ?hidden="${!this.showColorSchemeMode_()}">
      <div id="colorSchemeModeLabel" class="flex cr-padded-text"
          aria-hidden="true">
        $i18n{colorSchemeMode}
      </div>
      <select id="colorSchemeModeSelect" class="md-select"
          @change="${this.onColorSchemeModeChange_}"
          aria-labelledby="colorSchemeModeLabel">
        ${this.colorSchemeModeOptions_.map(item => html`
          <option value="${item.value}"
              ?selected="${this.isSelectedColorSchemeMode_(item.value)}">
            ${item.name}
          </option>
        `)}
      </select>
    </div>
    <div class="hr" ?hidden="${!this.showHr_(
        this.pageVisibility_.setTheme, this.pageVisibility_.homeButton)}">
    </div>
    <settings-toggle-button elide-label
        ?hidden="${!this.pageVisibility_.homeButton}"
        pref-key="browser.show_home_button"
        label="$i18n{showHomeButton}"
        sub-label="${this.getShowHomeSubLabel_()}">
    </settings-toggle-button>
    ${this.showHomeButtonPref_?.value ? html`
      <div id="home-button-options" class="list-frame"
          ?hidden="${!this.pageVisibility_.homeButton}">
        <settings-radio-group pref-key="homepage_is_newtabpage">
          <controlled-radio-button class="list-item" name="true"
              pref-key="homepage_is_newtabpage"
              label="$i18n{homePageNtp}" no-extension-indicator>
          </controlled-radio-button>
          <controlled-radio-button id="custom-input" class="list-item"
              name="false" pref-key="homepage_is_newtabpage"
              no-extension-indicator>
            <!-- TODO(dbeam): this can show double indicators when both
                 homepage and whether to use the NTP as the homepage are
                 managed. -->
            <home-url-input id="customHomePage"
                .canTab="${!this.homepageIsNewTabPagePref_?.value}">
            </home-url-input>
          </controlled-radio-button>
          ${this.homepagePref_?.extensionId ? html`
            <extension-controlled-indicator
                extension-id="${this.homepagePref_.extensionId}"
                ?extension-can-be-disabled="${!!this.homepagePref_?.extensionCanBeDisabled}"
                extension-name="${this.homepagePref_.controlledByName}"
                @disable-extension-click="${this.onDisableExtensionClick_}">
            </extension-controlled-indicator>
          ` : ''}
        </settings-radio-group>
      </div>
    ` : ''}
    <div
        class="hr"
        ?hidden="${!this.showHr_(
            this.pageVisibility_.homeButton, this.pageVisibility_.bookmarksBar)}">
    </div>
    ${!this.ntpSimplificationBookmarksBarEnabled_ ? html`
      <settings-toggle-button id="showBookmarksBar"
          ?hidden="${!this.pageVisibility_.bookmarksBar}"
          pref-key="bookmark_bar.show_on_all_tabs"
          label="$i18n{showBookmarksBar}">
      </settings-toggle-button>
    ` : ''}
    ${this.ntpSimplificationBookmarksBarEnabled_ ? html`
      <div class="cr-row" ?hidden="${!this.pageVisibility_.bookmarksBar}">
        <div class="flex cr-padded-text" aria-hidden="true">
          $i18n{bookmarksBar}
        </div>
        <settings-dropdown-menu id="bookmarksBarVisibilityDropdown"
            label="$i18n{bookmarksBar}"
            pref-key="bookmark_bar.visibility_state"
            .menuOptions="${this.bookmarksBarOptions_}"
            @settings-control-change="${this.onBookmarksBarVisibilitySettingsControlChange_}">
        </settings-dropdown-menu>
      </div>
    ` : ''}

    <if expr="is_macosx">
      ${this.showGlassEffectEnabled_ ? html`
        <div class="cr-row hr">
          <div class="flex cr-padded-text" aria-hidden="true">
            $i18n{glassEffect}
          </div>
          <settings-dropdown-menu id="glassEffect"
              label="$i18n{glassEffect}"
              pref-key="glass_frame.enabled"
              .menuOptions="${this.glassEffectOptions_}"
              @settings-control-change="${this.onGlassFrameSettingsControlChange_}">
          </settings-dropdown-menu>
        </div>
      ` : ''}
    </if>


    <div class="cr-row">
      <div class="flex cr-padded-text" aria-hidden="true">
        $i18n{tabStripPosition}
      </div>
      <settings-dropdown-menu id="tabStripPosition"
          label="$i18n{tabStripPosition}"
          pref-key="vertical_tabs.enabled"
          .menuOptions="${this.tabStripOptions_}"
          @settings-control-change="${this.onTabStripPositionSettingsControlChange_}">
      </settings-dropdown-menu>
    </div>
    <div class="list-frame indented-toggles"
        ?hidden="${!this.verticalTabsEnabledPref_?.value}">
      ${this.showVerticalTabsExpandOnHoverSetting_() ? html`
        <settings-toggle-button
            id="showVerticalTabsExpandOnHover"
            pref-key="vertical_tabs.expand_on_hover"
            label="$i18n{showVerticalTabsExpandOnHover}">
        </settings-toggle-button>
      ` : ''}

      ${this.showOrganizerPanelEnabled_ ? html`
        <settings-toggle-button id="showOrganizerPanelButton"
          pref-key="organizer_panel.pinned_to_tabstrip"
          label="$i18n{showOrganizerPanelButton}"
          @change="${this.onShowOrganizerPanelButtonChange_}">
        </settings-toggle-button>
      ` : ''}

      ${this.showEverythingMenuToggle_() ? html`
        <settings-toggle-button id="showEverythingMenuButton"
            pref-key="everything_menu.pinned_to_tabstrip"
            label="$i18n{showEverythingMenuButton}"
            @change="${this.onShowEverythingMenuButtonChange_}">
        </settings-toggle-button>
      ` : ''}
    </div>
    ${this.tabStripUnificationEnabled_ ? html`
      <div class="list-frame indented-toggles"
          ?hidden="${!!this.verticalTabsEnabledPref_?.value}">
        <settings-toggle-button id="tabScrollAutoShowOnOverflow"
            pref-key="tab_scroll_buttons.pinned_to_tabstrip"
            label="$i18n{tabScrollAutoShowOnOverflow}"
            @change="${this.onTabScrollAutoShowOnOverflowChange_}">
        </settings-toggle-button>
      </div>
    ` : ''}

    <settings-toggle-button id="showTabSearchButton" class="hr"
        pref-key="tab_search.pinned_to_tabstrip"
        label="$i18n{showTabSearchButton}"
        @change="${this.onShowTabSearchButtonChange_}">
    </settings-toggle-button>

    <settings-toggle-button class="hr" id="showSavedTabGroups"
        pref-key="bookmark_bar.show_tab_groups"
        label="$i18n{showTabGroupsInBookmarksBar}"
        ?hidden="${this.showOrganizerPanelEnabled_}">
    </settings-toggle-button>

    <settings-toggle-button class="hr" id="autoPinNewTabGroups"
        pref-key="auto_pin_new_tab_groups"
        label="$i18n{autoPinNewTabGroups}"
        ?hidden="${this.showOrganizerPanelEnabled_}">
    </settings-toggle-button>

    ${this.showCtrlTabMru_ ? html`
      <settings-toggle-button class="hr" id="ctrlTabMru"
          pref-key="browser.ctrl_tab_mru"
          label="$i18n{ctrlTabMru}">
      </settings-toggle-button>
    ` : ''}

    <div class="cr-row">$i18n{sidePanelPosition}</div>
    <div class="list-frame indented-rows">
      ${this.configurableSidePanels_.map(item => html`
        <div class="cr-row continuation">
          <div class="flex cr-padded-text" aria-hidden="true">${item.label}</div>
          <settings-dropdown-menu
              label="${this.i18n('sidePanelAlignmentA11yLabel', item.label,
                  '$i18n{sidePanelPosition}')}"
              .menuOptions="${this.sidePanelAlignmentOptions_}"
              data-entry-id="${item.id}"
              pref-key="side_panel.is_right_aligned"
              no-set-pref
              value="${this.getOverrideValue_(item.id)}"
              @settings-control-change="${this.onOverrideAlignmentSettingsControlChange_}">
          </settings-dropdown-menu>
        </div>
      `)}

      <div class="cr-row continuation">
        <div class="flex cr-padded-text" aria-hidden="true">
          $i18n{sidePanelAlignmentChromePanels}
        </div>
        <settings-dropdown-menu id="sidePanelPosition"
            label="${this.i18n('sidePanelAlignmentA11yLabel',
                '$i18n{sidePanelAlignmentChromePanels}',
                '$i18n{sidePanelPosition}')}"
            pref-key="side_panel.is_right_aligned"
            .menuOptions="${this.sidePanelAlignmentOptions_}">
        </settings-dropdown-menu>
      </div>
    </div>
    ${!this.showHoverCardImagesOption_ ? html`
      <div class="hr" ?hidden="${!this.pageVisibility_.hoverCard}"></div>
      <settings-toggle-button id="hoverCardMemoryUsageToggle"
          ?hidden="${!this.pageVisibility_.hoverCard}"
          pref-key="browser.hovercard.memory_usage_enabled"
          label="$i18n{showHoverCardMemoryUsageStandalone}">
      </settings-toggle-button>
    ` : ''}
    ${this.showHoverCardImagesOption_ ? html`
      <div class="cr-row" ?hidden="${!this.pageVisibility_.hoverCard}">
        $i18n{hoverCardTitle}
      </div>
      <div class="list-frame indented-toggles">
        <settings-toggle-button id="hoverCardImagesToggle"
            ?hidden="${!this.pageVisibility_.hoverCard}"
            @settings-boolean-control-change="${this.onHoverCardImagesSettingsBooleanControlChange_}"
            pref-key="browser.hovercard.image_previews_enabled"
            label="$i18n{showHoverCardImages}">
        </settings-toggle-button>
        <settings-toggle-button id="hoverCardMemoryUsageToggle" class="hr"
            ?hidden="${!this.pageVisibility_.hoverCard}"
            @settings-boolean-control-change="${this.onHoverCardMemoryUsageSettingsBooleanControlChange_}"
            pref-key="browser.hovercard.memory_usage_enabled"
            label="$i18n{showHoverCardMemoryUsage}">
        </settings-toggle-button>
      </div>
    ` : ''}

<if expr="is_linux">
    <div class="hr" ?hidden="${!this.pageVisibility_.bookmarksBar}"></div>
    <settings-toggle-button
        ?hidden="${!this.showCustomChromeFrame_}"
        pref-key="browser.custom_chrome_frame"
        label="$i18n{showWindowDecorations}"
        inverted>
    </settings-toggle-button>
</if>
    <div class="cr-row">
      <div class="flex cr-padded-text" aria-hidden="true">
        $i18n{fontSize}
      </div>
      <settings-dropdown-menu id="defaultFontSize" label="$i18n{fontSize}"
          pref-key="webkit.webprefs.default_font_size"
          .menuOptions="${this.fontSizeOptions_}">
      </settings-dropdown-menu>
    </div>
    <cr-link-row class="hr" id="customize-fonts-subpage-trigger"
        label="$i18n{customizeFonts}" @click="${this.onCustomizeFontsClick_}"
        role-description="$i18n{subpageArrowRoleDescription}">
    </cr-link-row>
    <div class="cr-row" ?hidden="${!this.pageVisibility_.pageZoom}">
      <div id="pageZoom" class="flex cr-padded-text" aria-hidden="true">
        $i18n{pageZoom}
      </div>
      <select id="zoomLevel" class="md-select" aria-labelledby="pageZoom"
          @change="${this.onZoomLevelChange_}">
        ${this.pageZoomLevels_.map(item => html`
          <option value="${item}"
              ?selected="${this.zoomValuesEqual_(item, this.defaultZoom_)}">
            ${this.formatZoom_(item)}%
          </option>
        `)}
      </select>
    </div>
<if expr="is_macosx">
    <settings-toggle-button class="hr"
        pref-key="webkit.webprefs.tabs_to_links"
        label="$i18n{tabsToLinks}">
    </settings-toggle-button>
    <settings-toggle-button class="hr"
        pref-key="browser.confirm_to_quit"
        label="$i18n{warnBeforeQuitting}">
    </settings-toggle-button>
</if>
  </div>
  <settings-toggle-button class="hr" id="splitViewDragAndDrop"
      pref-key="browser.split_view_drag_and_drop_enabled"
      label="${this.i18n(this.splitViewHorizontalEnabled_
               ? 'allowSplitViewDragAndDropHorizontal'
               : 'allowSplitViewDragAndDrop')}">
  </settings-toggle-button>
</settings-section>
${this.showManagedThemeDialog_ ? html`
  <managed-dialog @close="${this.onManagedDialogClose_}"
      title="$i18n{themeManagedDialogTitle}"
      body="$i18n{themeManagedDialogBody}">
  </managed-dialog>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
