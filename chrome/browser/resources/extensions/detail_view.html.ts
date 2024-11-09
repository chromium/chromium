// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ExtensionsDetailViewElement} from './detail_view.js';

export function getHtml(this: ExtensionsDetailViewElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<!-- Invisible instead of hidden because VoiceOver refuses to read text of
element that's hidden when referenced by an aria label. Unfortunately,
this text can be found by Ctrl + F because it isn't hidden. -->
<div id="a11yAssociation" aria-hidden="true">
  ${this.a11yAssociation(this.data.name)}
</div>
<div class="page-container" id="container">
  <div class="page-content">
    <div class="page-header">
      <cr-icon-button class="icon-arrow-back no-overlap" id="closeButton"
          aria-label="${this.getBackButtonAriaLabel_()}"
          aria-roledescription="${this.getBackButtonAriaRoleDescription_()}"
          @click="${this.onCloseButtonClick_}">
      </cr-icon-button>
      <img id="icon" src="${this.data.iconUrl}" alt="">
      <span id="name" class="cr-title-text" role="heading" aria-level="1">
        ${this.data.name}
      </span>
      ${!this.computeDevReloadButtonHidden_() ? html`
        <cr-icon-button id="dev-reload-button" class="icon-refresh no-overlap"
            title="$i18n{itemReload}" aria-label="$i18n{itemReload}"
            aria-describedby="a11yAssociation" @click="${this.onReloadClick_}">
        </cr-icon-button>` : ''}
    </div>

    ${this.showSafetyCheck_ ? html`
      <div id="safetyCheckWarningContainer" class="message-container">
        <cr-icon aria-hidden="true" icon="extensions-icons:my_extensions"
            class="message-icon">
        </cr-icon>
        <div class="message-text">
          <span class="section-title" aria-level="2">
            $i18n{safetyCheckExtensionsDetailPagePrimaryLabel}
          </span>
          <div class="section-content">
            ${this.data.safetyCheckText!.detailString}
          </div>
        </div>
        <cr-button class="keep-button" @click="${this.onKeepClick_}">
          $i18n{safetyCheckExtensionsKeep}
        </cr-button>
        <cr-button class="action-button" @click="${this.onRemoveClick_}">
          $i18n{remove}
        </cr-button>
      </div>` : ''}

    ${this.shouldShowMv2DeprecationMessage_() ? html`
      <div id="mv2DeprecationMessage" class="message-container">
        <cr-icon aria-hidden="true"
            icon="${this.getMv2DeprecationMessageIcon_()}" class="message-icon">
        </cr-icon>
        <div class="message-text">
          <span class="section-title" aria-level="2">
            ${this.getMv2DeprecationMessageHeader_()}
          </span>
          <div class="section-content"
              .innerHTML="${this.getMv2DeprecationMessageSubtitle_()}">
          </div>
        </div>
        <cr-button class="find-alternative-button"
            @click="${this.onFindAlternativeButtonClick_}"
            ?hidden="${!this.shouldShowMv2DeprecationFindAlternativeButton_()}">
          $i18n{mv2DeprecationPanelFindAlternativeButton}
        </cr-button>
        <cr-button class="remove-button" @click="${this.onRemoveButtonClick_}"
            ?hidden="${!this.shouldShowMv2DeprecationRemoveButton_()}">
          $i18n{mv2DeprecationMessageRemoveButton}
        </cr-button>
        <cr-icon-button class="icon-more-vert header-aligned-button"
            id="actionMenuButton" @click="${this.onActionMenuButtonClick_}"
            title="$i18n{moreOptions}"
            aria-label="${this.getActionMenuButtonLabel_()}"
            ?hidden="${!this.shouldShowMv2DeprecationActionMenu_()}">
        </cr-icon-button>
        <cr-action-menu id="actionMenu">
          <button class="dropdown-item" id="findAlternativeAction"
              ?hidden="${!this.
                  shouldShowMv2DeprecationFindAlternativeAction_()}"
              @click="${this.onFindAlternativeActionClick_}">
            $i18n{mv2DeprecationPanelFindAlternativeButton}
          </button>
          <button class="dropdown-item" id="keepAction"
              ?hidden="${!this.shouldShowMv2DeprecationKeepAction_()}"
              @click="${this.onKeepActionClick_}">
            $i18n{mv2DeprecationPanelKeepForNowButton}
          </button>
        </cr-action-menu>
      </div>` : ''}

    <div class="cr-row first control-line" id="enable-section">
      <span class="${this.computeEnabledStyle_()}">
        ${this.computeEnabledText_()}
      </span>
      <div class="layout-horizontal">
        <cr-tooltip-icon ?hidden="${!this.data.controlledInfo}"
            tooltip-text="${this.data.controlledInfo?.text || ''}"
            icon-class="cr20:domain"
            icon-aria-label="${this.data.controlledInfo?.text || ''}">
        </cr-tooltip-icon>
        ${this.showReloadButton_() ? html`
          <cr-button id="terminated-reload-button" class="action-button"
              @click="${this.onReloadClick_}">
            $i18n{itemReload}
          </cr-button>` : ''}
        <cr-tooltip-icon id="parentDisabledPermissionsToolTip"
            ?hidden="${!this.data.disableReasons.parentDisabledPermissions}"
            tooltip-text="$i18n{parentDisabledPermissions}"
            icon-class="cr20:kite"
            icon-aria-label="$i18n{parentDisabledPermissions}">
        </cr-tooltip-icon>
        <cr-toggle id="enableToggle"
            aria-label="${this.getEnableToggleAriaLabel_()}"
            aria-describedby="name enable-toggle-tooltip"
            ?checked="${this.isEnabled_()}"
            @change="${this.onEnableToggleChange_}"
            ?disabled="${!this.isEnableToggleEnabled_()}"
            ?hidden="${!this.showEnableToggle_()}">
        </cr-toggle>
        <cr-tooltip id="enable-toggle-tooltip" for="enableToggle"
            position="left" aria-hidden="true" animation-delay="0"
            fit-to-visible-bounds>
          ${this.getEnableToggleTooltipText_()}
        </cr-tooltip>
      </div>
    </div>
    ${this.hasSevereWarnings_() ? html`
      <div id="warnings">
        <div id="runtime-warnings"
            ?hidden="${!this.data.runtimeWarnings.length}"
            class="cr-row continuation warning control-line">
          <cr-icon class="warning-icon" icon="cr:error"></cr-icon>
          <span>
            ${this.data.runtimeWarnings.map(item => html`${item}`)}
          </span>
          ${!this.showReloadButton_() ? html`
            <cr-button id="warnings-reload-button" class="action-button"
                @click="${this.onReloadClick_}">
              $i18n{itemReload}
            </cr-button>` : ''}
        </div>
        <div class="cr-row continuation warning" id="suspicious-warning"
            ?hidden="${!this.data.disableReasons.suspiciousInstall}">
          <cr-icon class="warning-icon" icon="cr:warning"></cr-icon>
          <span>
            $i18n{itemSuspiciousInstall}
            <a target="_blank" href="$i18n{suspiciousInstallHelpUrl}"
                aria-label="$i18n{itemSuspiciousInstallLearnMore}">
              $i18n{learnMore}
            </a>
          </span>
        </div>
        <div class="cr-row continuation warning control-line"
            id="corrupted-warning" ?hidden="${!this.showRepairButton_()}">
          <cr-icon class="warning-icon" icon="cr:warning"></cr-icon>
          <span>$i18n{itemCorruptInstall}</span>
          <cr-button id="repair-button" class="action-button"
              @click="${this.onRepairClick_}">
            $i18n{itemRepair}
          </cr-button>
        </div>
        <div class="cr-row continuation warning" id="blocklisted-warning"
            ?hidden="${!this.shouldShowBlocklistText_()}">
          <cr-icon class="warning-icon" icon="cr:warning"></cr-icon>
          <span>${this.data.blocklistText}</span>
        </div>
        <div class="cr-row continuation warning" id="update-required-warning"
            ?hidden="${!this.data.disableReasons.updateRequired}">
          <cr-icon class="warning-icon" icon="cr:warning"></cr-icon>
          <span>$i18n{updateRequiredByPolicy}</span>
        </div>
        <div class="cr-row continuation warning"
            id="published-in-store-required-warning"
            ?hidden="${!this.data.disableReasons.publishedInStoreRequired}">
          <cr-icon class="warning-icon" icon="cr:warning"></cr-icon>
          <span>$i18n{publishedInStoreRequiredByPolicy}</span>
        </div>
      </div>` : ''}
    ${this.showAllowlistWarning_() ? html`
      <div id="allowlist-warning" class="cr-row continuation">
        <cr-icon class="warning-icon"
            icon="extensions-icons:safebrowsing_warning">
        </cr-icon>
        <span class="cr-secondary-text">
          $i18n{itemAllowlistWarning}
          <a href="$i18n{enhancedSafeBrowsingWarningHelpUrl}" target="_blank"
              aria-label="$i18n{itemAllowlistWarningLearnMoreLabel}">
            $i18n{learnMore}
          </a>
        </span>
      </div>` : ''}
    <div class="section">
      <div class="section-title" role="heading" aria-level="2">
        $i18n{itemDescriptionLabel}
      </div>
      <div class="section-content" id="description">
        ${this.getDescription_()}
      </div>
    </div>
    <div class="section hr">
      <div class="section-title" role="heading" aria-level="2">
        $i18n{itemVersion}
      </div>
      <div class="section-content">${this.data.version}</div>
    </div>
    <div class="section hr">
      <div class="section-title" role="heading" aria-level="2">
        $i18n{itemSize}
      </div>
      <div class="section-content" id="size">
        <span>${this.size_}</span>
        <div class="spinner" ?hidden="${!!this.size_}"></div>
      </div>
    </div>
    <div class="section hr" id="id-section" ?hidden="${!this.inDevMode}">
      <div class="section-title" role="heading" aria-level="2">
        $i18n{itemIdHeading}
      </div>
      <div class="section-content">${this.data.id}</div>
    </div>
    ${this.inDevMode ? html`
      <div class="section hr" id="inspectable-views">
        <div class="section-title" role="heading" aria-level="2">
          $i18n{itemInspectViews}
        </div>
        <div class="section-content">
          <ul id="inspect-views">
            <li ?hidden="${this.data.views.length}">
              $i18n{noActiveViews}
            </li>
            ${this.sortedViews_.map((item, index) => html`
              <li>
                <a is="action-link" class="inspectable-view"
                    data-index="${index}" @click="${this.onInspectClick_}">
                  ${this.computeInspectLabel_(item)}
                </a>
              </li>`)}
          </ul>
        </div>
      </div>` : ''}
    <div class="section hr">
      <div class="section-title" role="heading" aria-level="2">
        $i18n{itemPermissions}
      </div>
      <div class="section-content">
        <span id="no-permissions" ?hidden="${this.hasPermissions_()}">
          ${this.getNoPermissionsString_()}
        </span>
        <ul id="permissions-list"
            ?hidden="${!this.data.permissions.simplePermissions.length}">
          ${this.data.permissions.simplePermissions.map(item => html`
            <li>
              ${item.message}
              <ul ?hidden="${!item.submessages.length}">
                ${item.submessages.map(submessage => html`
                  <li>${submessage}</li>`)}
              </ul>
            </li>`)}
          <li ?hidden="${this.showSiteAccessSection_()}">
            $i18n{itemSiteAccessEmpty}
          </li>
        </ul>
      </div>
    </div>
    ${this.showSiteAccessSection_() ? html`
      <div class="section hr">
        <div class="section-title" role="heading" aria-level="2"
            ?hidden="${this.enableEnhancedSiteControls}">
          $i18n{itemSiteAccess}
        </div>
        <div class="section-content">
          <span id="no-site-access" ?hidden="${this.showSiteAccessContent_()}">
            $i18n{itemSiteAccessEmpty}
          </span>
          ${this.showFreeformRuntimeHostPermissions_() ? html`
            <extensions-runtime-host-permissions
                .permissions="${this.data.permissions.runtimeHostPermissions}"
                ?enable-enhanced-site-controls="${this
                    .enableEnhancedSiteControls}"
                .delegate="${this.delegate}" item-id="${this.data.id}">
            </extensions-runtime-host-permissions>` : ''}
          ${this.showHostPermissionsToggleList_() ? html`
            <extensions-host-permissions-toggle-list
                .permissions="${this.data.permissions.runtimeHostPermissions}"
                ?enable-enhanced-site-controls="${this.
                    enableEnhancedSiteControls}"
                .delegate="${this.delegate}" item-id="${this.data.id}">
            </extensions-host-permissions-toggle-list>` : ''}
          ${this.showEnableAccessRequestsToggle_() ? html`
            <extensions-toggle-row id="show-access-requests-toggle"
                ?checked="${this.data.showAccessRequestsInToolbar}"
                class="hr" @change="${this.onShowAccessRequestsChange_}">
              <div id="access-toggle-and-link">
                <span>$i18n{itemShowAccessRequestsInToolbar}</span>
                <a class="link-icon-button"
                    aria-label="$i18n{itemShowAccessRequestsLearnMore}"
                    href="$i18n{showAccessRequestsInToolbarLearnMoreLink}"
                    target="_blank">
                  <cr-icon icon="cr:help-outline"></cr-icon>
                </a>
              </div>
            </extensions-toggle-row>` : ''}
        </div>
      </div>` : ''}
    ${this.hasDependentExtensions_() ? html`
      <div class="section hr">
        <div class="section-title" role="heading" aria-level="2">
          $i18n{itemDependencies}
        </div>
        <div class="section-content">
          <ul id="dependent-extensions-list">
            ${this.data.dependentExtensions.map(item => html`
              <li>${this.computeDependentEntry_(item)}</li>`)}
          </ul>
        </div>
      </div>` : ''}
    <cr-link-row class="hr" id="siteSettings" label="$i18n{siteSettings}"
        @click="${this.onSiteSettingsClick_}" external></cr-link-row>
    ${this.shouldShowOptionsSection_() ? html`
      <div id="options-section">
        ${this.canPinToToolbar_() ? html`
          <extensions-toggle-row id="pin-to-toolbar"
              ?checked="${this.data.pinnedToToolbar}" class="hr"
              @change="${this.onPinnedToToolbarChange_}">
            <span>$i18n{itemPinToToolbar}</span>
          </extensions-toggle-row>` : ''}
        ${this.shouldShowIncognitoOption_() ? html`
          <extensions-toggle-row id="allow-incognito"
              ?checked="${this.data.incognitoAccess.isActive}" class="hr"
              @change="${this.onAllowIncognitoChange_}">
            <div>
              <div>$i18n{itemAllowIncognito}</div>
              <div class="section-content">$i18n{incognitoInfoWarning}</div>
            </div>
          </extensions-toggle-row>` : ''}
        ${this.data.fileAccess.isEnabled ? html`
          <extensions-toggle-row id="allow-on-file-urls"
              ?checked="${this.data.fileAccess.isActive}" class="hr"
              @change="${this.onAllowOnFileUrlsChange_}">
            <span>$i18n{itemAllowOnFileUrls}</span>
          </extensions-toggle-row>` : ''}
        ${this.data.errorCollection.isEnabled ? html`
          <extensions-toggle-row id="collect-errors"
              ?checked="${this.data.errorCollection.isActive}" class="hr"
              @change="${this.onCollectErrorsChange_}">
            <span>$i18n{itemCollectErrors}</span>
          </extensions-toggle-row>` : ''}
      </div>` : ''}
    <cr-link-row class="hr" id="extensionsOptions"
        ?disabled="${!this.isEnabled_()}"
        ?hidden="${!this.shouldShowOptionsLink_()}"
        label="$i18n{itemOptions}" @click="${this.onExtensionOptionsClick_}"
        external>
    </cr-link-row>
    <cr-link-row class="hr" id="extensionsActivityLogLink"
        ?hidden="${!this.showActivityLog}" label="$i18n{viewActivityLog}"
        @click="${this.onActivityLogClick_}"
        role-description="$i18n{subpageArrowRoleDescription}">
    </cr-link-row>
    <cr-link-row class="hr" ?hidden="${!this.data.manifestHomePageUrl.length}"
        id="extensionWebsite" label="$i18n{extensionWebsite}"
        @click="${this.onExtensionWebSiteClick_}" external>
    </cr-link-row>
    <cr-link-row class="hr" ?hidden="${!this.data.webStoreUrl.length}"
        id="viewInStore" label="$i18n{viewInStore}"
        @click="${this.onViewInStoreClick_}" external>
    </cr-link-row>
    <div class="section hr">
      <div class="section-title" role="heading" aria-level="2">
        $i18n{itemSource}
      </div>
      <div id="source" class="section-content">
        ${this.computeSourceString_()}
      </div>
      <div id="load-path" class="section-content"
          ?hidden="${!this.data.prettifiedPath}">
        <span>$i18n{itemExtensionPath}</span>
        <a is="action-link" @click="${this.onLoadPathClick_}">
          ${this.data.prettifiedPath}
        </a>
      </div>
    </div>
    <cr-link-row class="hr" id="remove-extension"
        ?hidden="${this.data.mustRemainInstalled}"
        label="$i18n{itemRemoveExtension}" @click="${this.onRemoveClick_}">
    </cr-link-row>
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
