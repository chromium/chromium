// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './host_permissions_toggle_list.js';
import './runtime_host_permissions.js';
import '/strings.m.js';
import './toggle_row.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import type {CrTooltipIconElement} from 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './detail_view.css.js';
import {getHtml} from './detail_view.html.js';
import type {ItemDelegate} from './item.js';
import {DummyItemDelegate} from './item.js';
import {ItemMixin} from './item_mixin.js';
import {computeInspectableViewLabel, convertSafetyCheckReason, createDummyExtensionInfo, EnableControl, getEnableControl, getEnableToggleAriaLabel, getEnableToggleTooltipText, getItemSource, getItemSourceString, isEnabled, SAFETY_HUB_EXTENSION_KEPT_HISTOGRAM_NAME, SAFETY_HUB_EXTENSION_REMOVED_HISTOGRAM_NAME, SAFETY_HUB_WARNING_REASON_MAX_SIZE, sortViews, userCanChangeEnablement} from './item_util.js';
import type {Mv2DeprecationDelegate} from './mv2_deprecation_delegate.js';
import {getMv2ExperimentStage, Mv2ExperimentStage} from './mv2_deprecation_util.js';
import {navigation, Page} from './navigation_helper.js';
import type {ExtensionsToggleRowElement} from './toggle_row.js';

class DummyDetailViewDelegate extends DummyItemDelegate {
  dismissMv2DeprecationNotice() {}
  dismissMv2DeprecationNoticeForExtension(_id: string) {}
}

export interface ExtensionsDetailViewElement {
  $: {
    actionMenu: CrActionMenuElement,
    closeButton: HTMLElement,
    description: HTMLElement,
    enableToggle: CrToggleElement,
    extensionsActivityLogLink: HTMLElement,
    extensionsOptions: CrLinkRowElement,
    parentDisabledPermissionsToolTip: CrTooltipIconElement,
    safetyCheckWarningContainer: HTMLElement,
    source: HTMLElement,
  };
}

const ExtensionsDetailViewElementBase = I18nMixinLit(ItemMixin(CrLitElement));

export class ExtensionsDetailViewElement extends
    ExtensionsDetailViewElementBase {
  static get is() {
    return 'extensions-detail-view';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The underlying ExtensionInfo for the details being displayed.
       */
      data: {type: Object},

      size_: {type: String},

      delegate: {type: Object},

      /** Whether the user has enabled the UI's developer mode. */
      inDevMode: {type: Boolean},

      /**
       * Whether enhanced site controls have been enabled (through a feature
       * flag). For this page, there are some changes to the site permissions
       * section.
       */
      enableEnhancedSiteControls: {type: Boolean},

      /** Whether "allow in incognito" option should be shown. */
      incognitoAvailable: {type: Boolean},

      /** Whether "View Activity Log" link should be shown. */
      showActivityLog: {type: Boolean},

      /** Whether the user navigated to this page from the activity log page. */
      fromActivityLog: {type: Boolean},

      /** Inspectable views sorted to put background/service workers first */
      sortedViews_: {type: Array},

      /** Whether the extensions safety check warning is shown. */
      showSafetyCheck_: {type: Boolean},

      /**
       * Current Manifest V2 experiment stage.
       */
      mv2ExperimentStage_: {
        type: Number,
        state: true,
      },
    };
  }

  data: chrome.developerPrivate.ExtensionInfo = createDummyExtensionInfo();
  delegate: ItemDelegate&Mv2DeprecationDelegate = new DummyDetailViewDelegate();
  inDevMode: boolean = false;
  enableEnhancedSiteControls: boolean = false;
  incognitoAvailable: boolean = false;
  showActivityLog: boolean = false;
  fromActivityLog: boolean = false;
  protected showSafetyCheck_: boolean = false;
  protected size_: string = '';
  protected sortedViews_: chrome.developerPrivate.ExtensionView[] = [];
  private mv2ExperimentStage_: Mv2ExperimentStage =
      getMv2ExperimentStage(loadTimeData.getInteger('MV2ExperimentStage'));

  override firstUpdated() {
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('data')) {
      this.sortedViews_ = sortViews(this.data.views);
      this.showSafetyCheck_ = this.computeShowSafetyCheck_();
    }

    if (changedProperties.has('data') || changedProperties.has('delegate')) {
      this.onItemIdChanged_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('showSafetyCheck_') &&
        this.showSafetyCheck_) {
      chrome.metricsPrivate.recordUserAction('SafetyCheck.DetailWarningShown');
    }
  }

  /**
   * Focuses the extensions options button. This should be used after the
   * dialog closes.
   */
  focusOptionsButton() {
    this.$.extensionsOptions.focus();
  }

  /**
   * Focuses the back button when page is loaded.
   */
  private async onViewEnterStart_() {
    const elementToFocus = this.fromActivityLog ?
        this.$.extensionsActivityLogLink :
        this.$.closeButton;

    await this.updateComplete;
    focusWithoutInk(elementToFocus);
  }

  private onItemIdChanged_() {
    // Clear the size, since this view is reused, such that no obsolete size
    // is displayed.:
    this.size_ = '';
    this.delegate.getExtensionSize(this.data.id).then(size => {
      this.size_ = size;
    });
  }

  protected onActivityLogClick_() {
    navigation.navigateTo({page: Page.ACTIVITY_LOG, extensionId: this.data.id});
  }

  protected getDescription_(): string {
    return this.data.description || loadTimeData.getString('noDescription');
  }

  protected getBackButtonAriaLabel_(): string {
    return loadTimeData.getStringF(
        'itemDetailsBackButtonAriaLabel', this.data.name);
  }

  protected getBackButtonAriaRoleDescription_(): string {
    return loadTimeData.getStringF(
        'itemDetailsBackButtonRoleDescription', this.data.name);
  }

  protected getEnableToggleAriaLabel_(): string {
    return getEnableToggleAriaLabel(
        this.isEnabled_(), this.data.type, this.i18n('appEnabled'),
        this.i18n('extensionEnabled'), this.i18n('itemOff'));
  }

  protected getEnableToggleTooltipText_(): string {
    return getEnableToggleTooltipText(this.data);
  }

  protected onCloseButtonClick_() {
    navigation.navigateTo({page: Page.LIST});
  }

  protected isEnabled_(): boolean {
    return isEnabled(this.data.state);
  }

  protected isEnableToggleEnabled_(): boolean {
    return userCanChangeEnablement(this.data, this.mv2ExperimentStage_);
  }

  protected hasDependentExtensions_(): boolean {
    return this.data.dependentExtensions.length > 0;
  }

  protected hasSevereWarnings_(): boolean {
    return this.data.disableReasons.corruptInstall ||
        this.data.disableReasons.suspiciousInstall ||
        this.data.disableReasons.updateRequired || !!this.data.blocklistText ||
        this.data.disableReasons.publishedInStoreRequired ||
        this.data.runtimeWarnings.length > 0;
  }

  protected computeDevReloadButtonHidden_(): boolean {
    return !this.canReloadItem();
  }

  protected computeEnabledStyle_(): string {
    return this.isEnabled_() ? 'enabled-text' : '';
  }

  protected computeEnabledText_(): string {
    // TODO(devlin): Get the full spectrum of these strings from bettes.
    return loadTimeData.getString(
        isEnabled(this.data.state) ? 'itemOn' : 'itemOff');
  }

  protected computeInspectLabel_(view: chrome.developerPrivate.ExtensionView):
      string {
    return computeInspectableViewLabel(view);
  }

  protected shouldShowOptionsLink_(): boolean {
    return !!this.data.optionsPage;
  }

  protected shouldShowOptionsSection_(): boolean {
    return this.canPinToToolbar_() || this.data.incognitoAccess.isEnabled ||
        this.data.fileAccess.isEnabled || this.data.errorCollection.isEnabled;
  }

  protected canPinToToolbar_(): boolean {
    return this.data.pinnedToToolbar !== undefined;
  }

  protected shouldShowIncognitoOption_(): boolean {
    return this.data.incognitoAccess.isEnabled && this.incognitoAvailable;
  }

  protected onEnableToggleChange_() {
    this.delegate.setItemEnabled(this.data.id, this.$.enableToggle.checked);
    this.$.enableToggle.checked = this.isEnabled_();
  }

  protected onInspectClick_(e: Event) {
    const index = Number((e.target as HTMLElement).dataset['index']);
    this.delegate.inspectItemView(this.data.id, this.sortedViews_[index]!);
  }

  protected onExtensionOptionsClick_() {
    this.delegate.showItemOptionsPage(this.data);
  }

  protected onReloadClick_() {
    this.reloadItem().catch((loadError) => this.fire('load-error', loadError));
  }

  protected onRemoveClick_() {
    if (this.showSafetyCheck_) {
      chrome.metricsPrivate.recordUserAction('SafetyCheck.DetailRemoveClicked');
      chrome.metricsPrivate.recordEnumerationValue(
          SAFETY_HUB_EXTENSION_REMOVED_HISTOGRAM_NAME,
          convertSafetyCheckReason(this.data.safetyCheckWarningReason),
          SAFETY_HUB_WARNING_REASON_MAX_SIZE);
    }
    this.delegate.deleteItem(this.data.id);
  }

  protected onKeepClick_() {
    if (this.showSafetyCheck_) {
      chrome.metricsPrivate.recordUserAction('SafetyCheck.DetailKeepClicked');
      chrome.metricsPrivate.recordEnumerationValue(
          SAFETY_HUB_EXTENSION_KEPT_HISTOGRAM_NAME,
          convertSafetyCheckReason(this.data.safetyCheckWarningReason),
          SAFETY_HUB_WARNING_REASON_MAX_SIZE);
    }
    this.delegate.setItemSafetyCheckWarningAcknowledged(
        this.data.id, this.data.safetyCheckWarningReason);
  }

  /**
   * Opens a URL in the Web Store with extensions recommendations for the
   * extension.
   */
  protected onFindAlternativeButtonClick_(): void {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Mv2Deprecation.Warning.FindAlternativeForExtension.Entry');
    const recommendationsUrl: string|undefined = this.data.recommendationsUrl;
    assert(!!recommendationsUrl);
    this.delegate.openUrl(recommendationsUrl);
  }

  /**
   * Triggers the extension's removal.
   */
  protected onRemoveButtonClick_(): void {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
      case Mv2ExperimentStage.WARNING:
        assertNotReached();
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.DisableWithReEnable.Remove');
        break;
      case Mv2ExperimentStage.UNSUPPORTED:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Unsupported.RemoveExtension.DetailPage');
        break;
    }

    this.delegate.deleteItem(this.data.id);
  }

  protected onRepairClick_() {
    this.delegate.repairItem(this.data.id);
  }

  protected onLoadPathClick_() {
    this.delegate.showInFolder(this.data.id);
  }

  protected onPinnedToToolbarChange_() {
    this.delegate.setItemPinnedToToolbar(
        this.data.id,
        this.shadowRoot!
            .querySelector<ExtensionsToggleRowElement>(
                '#pin-to-toolbar')!.checked);
  }

  protected onAllowIncognitoChange_() {
    this.delegate.setItemAllowedIncognito(
        this.data.id,
        this.shadowRoot!
            .querySelector<ExtensionsToggleRowElement>(
                '#allow-incognito')!.checked);
  }

  protected onAllowOnFileUrlsChange_() {
    this.delegate.setItemAllowedOnFileUrls(
        this.data.id,
        this.shadowRoot!
            .querySelector<ExtensionsToggleRowElement>(
                '#allow-on-file-urls')!.checked);
  }

  protected onCollectErrorsChange_() {
    this.delegate.setItemCollectsErrors(
        this.data.id,
        this.shadowRoot!
            .querySelector<ExtensionsToggleRowElement>(
                '#collect-errors')!.checked);
  }

  protected onExtensionWebSiteClick_() {
    this.delegate.openUrl(this.data.manifestHomePageUrl);
  }

  protected onSiteSettingsClick_() {
    this.delegate.openUrl(
        `chrome://settings/content/siteDetails?site=chrome-extension://${
            this.data.id}`);
  }

  protected onViewInStoreClick_() {
    this.delegate.openUrl(this.data.webStoreUrl);
  }

  protected computeDependentEntry_(
      item: chrome.developerPrivate.DependentExtension): string {
    return loadTimeData.getStringF('itemDependentEntry', item.name, item.id);
  }

  protected computeSourceString_(): string {
    return this.data.locationText ||
        getItemSourceString(getItemSource(this.data));
  }

  protected hasPermissions_(): boolean {
    return this.data.permissions.simplePermissions.length > 0 ||
        this.hasRuntimeHostPermissions_();
  }

  protected getNoPermissionsString_(): string {
    const showPermissionsAndSiteAccessStrings =
        this.enableEnhancedSiteControls && !this.showSiteAccessContent_();
    return loadTimeData.getString(
        showPermissionsAndSiteAccessStrings ?
            'itemPermissionsAndSiteAccessEmpty' :
            'itemPermissionsEmpty');
  }

  private hasRuntimeHostPermissions_(): boolean {
    return !!this.data.permissions.runtimeHostPermissions;
  }

  // Returns whether the site access section should be shown. This includes the
  // "no site access" message shown in the section if
  // |enableEnhancedSiteControls| is not enabled.
  protected showSiteAccessSection_(): boolean {
    return !this.enableEnhancedSiteControls || this.showSiteAccessContent_();
  }

  protected showSiteAccessContent_(): boolean {
    return this.showFreeformRuntimeHostPermissions_() ||
        this.showHostPermissionsToggleList_();
  }

  protected showFreeformRuntimeHostPermissions_(): boolean {
    return this.hasRuntimeHostPermissions_() &&
        this.data.permissions.runtimeHostPermissions!.hasAllHosts;
  }

  protected showHostPermissionsToggleList_(): boolean {
    return this.hasRuntimeHostPermissions_() &&
        !this.data.permissions.runtimeHostPermissions!.hasAllHosts;
  }

  protected showEnableAccessRequestsToggle_(): boolean {
    return this.showSiteAccessContent_() && this.enableEnhancedSiteControls;
  }

  protected onShowAccessRequestsChange_() {
    const showAccessRequestsToggle =
        this.shadowRoot!.querySelector<ExtensionsToggleRowElement>(
            '#show-access-requests-toggle');
    assert(showAccessRequestsToggle);
    this.delegate.setShowAccessRequestsInToolbar(
        this.data.id, showAccessRequestsToggle.checked);
  }

  protected showReloadButton_(): boolean {
    return getEnableControl(this.data) === EnableControl.RELOAD;
  }

  private computeShowSafetyCheck_(): boolean {
    if (!loadTimeData.getBoolean('safetyCheckShowReviewPanel')) {
      return false;
    }
    const ExtensionType = chrome.developerPrivate.ExtensionType;
    // Check to make sure this is an extension and not a Chrome app.
    if (!(this.data.type === ExtensionType.EXTENSION ||
          this.data.type === ExtensionType.SHARED_MODULE)) {
      return false;
    }
    return !!(
        this.data.safetyCheckText && this.data.safetyCheckText.detailString);
  }

  /**
   * Returns whether the mv2 deprecation message should be displayed.
   */
  protected shouldShowMv2DeprecationMessage_(): boolean {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
        return false;
      case Mv2ExperimentStage.WARNING:
        return this.data.isAffectedByMV2Deprecation;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        return this.data.isAffectedByMV2Deprecation &&
            this.data.disableReasons.unsupportedManifestVersion &&
            !this.data.didAcknowledgeMV2DeprecationNotice;
      case Mv2ExperimentStage.UNSUPPORTED:
        return this.data.isAffectedByMV2Deprecation &&
          this.data.disableReasons.unsupportedManifestVersion;
      default:
        assertNotReached();
    }
  }

  /**
   * Returns whether the find alternative button in the mv2 deprecation message
   * should be displayed.
   */
  protected shouldShowMv2DeprecationFindAlternativeButton_(): boolean {
    return this.mv2ExperimentStage_ === Mv2ExperimentStage.WARNING &&
        !!this.data.recommendationsUrl;
  }

  /**
   * Returns whether the remove button in the mv2 deprecation message should be
   * displayed.
   */
  protected shouldShowMv2DeprecationRemoveButton_(): boolean {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
      case Mv2ExperimentStage.WARNING:
        return false;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        return !this.data.mustRemainInstalled;
    }
  }

  /**
   * Returns whether the action menu button in the mv2 deprecation message
   * should be displayed.
   */
  protected shouldShowMv2DeprecationActionMenu_(): boolean {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
      case Mv2ExperimentStage.WARNING:
        return false;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        return true;
      case Mv2ExperimentStage.UNSUPPORTED:
        // 'Find alternative' is the only action for this stage. Thus, we only
        // show the menu if the action should be visible. For UNSUPPORTED, this
        // is when the recommendationsUrl is non-empty.
        return !!this.data.recommendationsUrl;
    }
  }

  /**
   * Returns whether the find alternative button in mv2 deprecation message
   * action menu should be displayed.
   */
  protected shouldShowMv2DeprecationFindAlternativeAction_(): boolean {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
      case Mv2ExperimentStage.WARNING:
        return false;
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        return !!this.data.recommendationsUrl;
    }
  }

  /**
   * Returns whether the keep button in mv2 deprecation message action menu
   * should be displayed.
   */
  protected shouldShowMv2DeprecationKeepAction_(): boolean {
    return this.mv2ExperimentStage_ ===
        Mv2ExperimentStage.DISABLE_WITH_REENABLE;
  }

  protected shouldShowBlocklistText_(): boolean {
    return !this.showSafetyCheck_ && !!this.data.blocklistText;
  }

  protected showRepairButton_(): boolean {
    return getEnableControl(this.data) === EnableControl.REPAIR;
  }

  protected showEnableToggle_(): boolean {
    const enableControl = getEnableControl(this.data);
    // We still show the toggle even if we also show the repair button in the
    // detail view, because the repair button appears just beneath it.
    return enableControl === EnableControl.ENABLE_TOGGLE ||
        enableControl === EnableControl.REPAIR;
  }

  protected showAllowlistWarning_(): boolean {
    // Only show the allowlist warning if there is no blocklist warning. It
    // would be redundant since all blocklisted items are necessarily not
    // included in the Safe Browsing allowlist.
    return this.data.showSafeBrowsingAllowlistWarning &&
        !this.data.blocklistText;
  }

  /** Opens the action menu for the extension. */
  protected onActionMenuButtonClick_(event: MouseEvent): void {
    this.$.actionMenu.showAt(
        event.target as HTMLElement,
        {anchorAlignmentY: AnchorAlignment.AFTER_END});
  }

  /**
   * Opens a URL in the Web Store with extensions recommendations for the
   * extension.
   */
  protected onFindAlternativeActionClick_(): void {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
      case Mv2ExperimentStage.WARNING:
        assertNotReached();
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Disabled.FindAlternativeForExtension.DetailPage');
        break;
      case Mv2ExperimentStage.UNSUPPORTED:
        chrome.metricsPrivate.recordUserAction(
            'Extensions.Mv2Deprecation.Unsupported.FindAlternativeForExtension.DetailPage');
        break;
    }

    this.$.actionMenu.close();

    const recommendationsUrl: string|undefined = this.data.recommendationsUrl;
    assert(!!recommendationsUrl);
    this.delegate.openUrl(recommendationsUrl);
  }

  /**
   * Dismisses the notice for a given extension in the disable experiment stage.
   * It will not be shown again during this stage.
   */
  protected onKeepActionClick_(): void {
    assert(
        this.mv2ExperimentStage_ === Mv2ExperimentStage.DISABLE_WITH_REENABLE);
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Mv2Deprecation.Disabled.DismissedForExtension.DetailPage');
    this.$.actionMenu.close();
    this.delegate.dismissMv2DeprecationNoticeForExtension(this.data.id);
  }

  /**
   * Returns the Manifest V2 deprecation message header.
   */
  protected getMv2DeprecationMessageHeader_(): string {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
        return '';
      case Mv2ExperimentStage.WARNING:
        return this.i18n('mv2DeprecationMessageWarningHeader');
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        return this.i18n('mv2DeprecationMessageDisabledHeader');
      default:
        assertNotReached();
    }
  }

  /**
   * Returns the HTML representation of the Manifest V2 deprecation message
   * subtitle string. We need the HTML representation instead of the string
   * since the string holds a link.
   */
  protected getMv2DeprecationMessageSubtitle_(): TrustedHTML {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
        return window.trustedTypes!.emptyHTML;
      case Mv2ExperimentStage.WARNING:
        return this.i18nAdvanced('mv2DeprecationMessageWarningSubtitle', {
          substitutions:
              ['https://chromewebstore.google.com/category/extensions'],
        });
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        return this.i18nAdvanced('mv2DeprecationMessageDisabledSubtitle', {
          substitutions: [
            'https://support.google.com/chrome_webstore' +
                '?p=unsupported_extensions',
          ],
        });
      default:
        assertNotReached();
    }
  }

  /**
   * Returns the Manifest V2 deprecation message icon.
   */
  protected getMv2DeprecationMessageIcon_(): string {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
      case Mv2ExperimentStage.WARNING:
        return 'extensions-icons:my_extensions';
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        return 'extensions-icons:extension_off';
      default:
        assertNotReached();
    }
  }

  /** Returns the accessible label for the action menu button */
  protected getActionMenuButtonLabel_(): string {
    return this.i18n(
        'mv2DeprecationPanelExtensionActionMenuLabel', this.data.name);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-detail-view': ExtensionsDetailViewElement;
  }
}

customElements.define(
    ExtensionsDetailViewElement.is, ExtensionsDetailViewElement);
