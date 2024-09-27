// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/js/action_link.js';
import './icons.html.js';
import './strings.m.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './item.css.js';
import {getHtml} from './item.html.js';
import {ItemMixinLit} from './item_mixin_lit.js';
import {computeInspectableViewLabel, createDummyExtensionInfo, EnableControl, getEnableControl, getEnableToggleAriaLabel, getEnableToggleTooltipText, getItemSource, getItemSourceString, isEnabled, sortViews, SourceType, userCanChangeEnablement} from './item_util.js';
import {Mv2ExperimentStage} from './mv2_deprecation_util.js';
import {navigation, Page} from './navigation_helper.js';

export interface ItemDelegate {
  deleteItem(id: string): void;
  deleteItems(ids: string[]): Promise<void>;
  uninstallItem(id: string): Promise<void>;
  setItemEnabled(id: string, isEnabled: boolean): void;
  setItemAllowedIncognito(id: string, isAllowedIncognito: boolean): void;
  setItemAllowedOnFileUrls(id: string, isAllowedOnFileUrls: boolean): void;
  setItemHostAccess(id: string, hostAccess: chrome.developerPrivate.HostAccess):
      void;
  setItemCollectsErrors(id: string, collectsErrors: boolean): void;
  inspectItemView(id: string, view: chrome.developerPrivate.ExtensionView):
      void;
  openUrl(url: string): void;
  reloadItem(id: string): Promise<void>;
  repairItem(id: string): void;
  showItemOptionsPage(extension: chrome.developerPrivate.ExtensionInfo): void;
  showInFolder(id: string): void;
  getExtensionSize(id: string): Promise<string>;
  addRuntimeHostPermission(id: string, host: string): Promise<void>;
  removeRuntimeHostPermission(id: string, host: string): Promise<void>;
  setItemSafetyCheckWarningAcknowledged(
      id: string,
      reason: chrome.developerPrivate.SafetyCheckWarningReason): void;
  setShowAccessRequestsInToolbar(id: string, showRequests: boolean): void;
  setItemPinnedToToolbar(id: string, pinnedToToolbar: boolean): void;

  // TODO(tjudkins): This function is not specific to items, so should be pulled
  // out to a more generic place when we need to access it from elsewhere.
  recordUserAction(metricName: string): void;
  getItemStateChangedTarget():
      ChromeEvent<(data: chrome.developerPrivate.EventData) => void>;
}

export interface ExtensionsItemElement {
  $: {
    a11yAssociation: HTMLElement,
    detailsButton: HTMLElement,
    enableToggle: CrToggleElement,
    name: HTMLElement,
    removeButton: HTMLElement,
  };
}

const ExtensionsItemElementBase = I18nMixinLit(ItemMixinLit(CrLitElement));

export class ExtensionsItemElement extends ExtensionsItemElementBase {
  static get is() {
    return 'extensions-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // The item's delegate, or null.
      delegate: {type: Object},

      // Whether or not dev mode is enabled.
      inDevMode: {type: Boolean},
      safetyCheckShowing: {type: Boolean},

      // The underlying ExtensionInfo itself. Public for use in declarative
      // bindings.
      data: {type: Object},

      mv2ExperimentStage: {type: Number},

      // Whether or not the expanded view of the item is shown.
      showingDetails_: {type: Boolean},

      // First inspectable view after sorting.
      firstInspectView_: {type: Object},
    };
  }

  delegate: ItemDelegate|null = null;
  inDevMode: boolean = false;
  mv2ExperimentStage: Mv2ExperimentStage = Mv2ExperimentStage.NONE;
  safetyCheckShowing: boolean = false;
  data: chrome.developerPrivate.ExtensionInfo = createDummyExtensionInfo();
  private showingDetails_: boolean = false;
  private firstInspectView_: chrome.developerPrivate.ExtensionView;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('data')) {
      this.firstInspectView_ = this.computeFirstInspectView_();
    }
  }

  /** @return The "Details" button. */
  getDetailsButton(): HTMLElement {
    return this.$.detailsButton;
  }

  /** @return The "Remove" button, if it exists. */
  getRemoveButton(): HTMLElement|null {
    return this.data.mustRemainInstalled ? null : this.$.removeButton;
  }

  /** @return The "Errors" button, if it exists. */
  getErrorsButton(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#errors-button');
  }

  protected getEnableToggleAriaLabel_(): string {
    return getEnableToggleAriaLabel(
        this.isEnabled_(), this.data.type, this.i18n('appEnabled'),
        this.i18n('extensionEnabled'), this.i18n('itemOff'));
  }

  protected getEnableToggleTooltipText_(): string {
    return getEnableToggleTooltipText(this.data);
  }

  protected getIdElementText_() {
    return this.i18n('itemId', this.data.id);
  }

  protected shouldShowErrorsButton_(): boolean {
    // When the error console is disabled (happens when
    // --disable-error-console command line flag is used or when in the
    // Stable/Beta channel), |installWarnings| is populated.
    if (this.data.installWarnings && this.data.installWarnings.length > 0) {
      return true;
    }

    // When error console is enabled |installedWarnings| is not populated.
    // Instead |manifestErrors| and |runtimeErrors| are used.
    return this.data.manifestErrors.length > 0 ||
        this.data.runtimeErrors.length > 0;
  }

  protected onRemoveClick_() {
    if (this.safetyCheckShowing) {
      const actionToRecord = this.data.safetyCheckText ?
          'SafetyCheck.ReviewPanelRemoveClicked' :
          'SafetyCheck.NonTriggeringExtensionRemoved';
      chrome.metricsPrivate.recordUserAction(actionToRecord);
    }
    assert(this.delegate);
    this.delegate.deleteItem(this.data.id);
  }

  protected onEnableToggleChange_() {
    assert(this.delegate);
    this.delegate.setItemEnabled(this.data.id, this.$.enableToggle.checked);
    this.$.enableToggle.checked = this.isEnabled_();
  }

  protected onErrorsClick_() {
    if (this.data.installWarnings && this.data.installWarnings.length > 0) {
      this.fire('show-install-warnings', this.data.installWarnings);
      return;
    }

    navigation.navigateTo({page: Page.ERRORS, extensionId: this.data.id});
  }

  protected onDetailsClick_() {
    navigation.navigateTo({page: Page.DETAILS, extensionId: this.data.id});
  }

  private computeFirstInspectView_(): chrome.developerPrivate.ExtensionView {
    return sortViews(this.data.views)[0];
  }

  protected onInspectClick_() {
    assert(this.delegate);
    this.delegate.inspectItemView(this.data.id, this.firstInspectView_);
  }

  protected onExtraInspectClick_() {
    navigation.navigateTo({page: Page.DETAILS, extensionId: this.data.id});
  }

  protected onReloadClick_() {
    this.reloadItem().catch((loadError) => this.fire('load-error', loadError));
  }

  protected onRepairClick_() {
    assert(this.delegate);
    this.delegate.repairItem(this.data.id);
  }

  protected isEnabled_(): boolean {
    return isEnabled(this.data.state);
  }

  protected isEnableToggleEnabled_(): boolean {
    return userCanChangeEnablement(this.data, this.mv2ExperimentStage);
  }

  /** @return Whether the reload button should be shown. */
  protected showReloadButton_(): boolean {
    return getEnableControl(this.data) === EnableControl.RELOAD;
  }

  /** @return Whether the repair button should be shown. */
  protected showRepairButton_(): boolean {
    return getEnableControl(this.data) === EnableControl.REPAIR;
  }


  /** @return Whether the enable toggle should be shown. */
  protected showEnableToggle_(): boolean {
    return getEnableControl(this.data) === EnableControl.ENABLE_TOGGLE;
  }

  protected computeClasses_(): string {
    let classes = this.isEnabled_() ? 'enabled' : 'disabled';
    if (this.inDevMode) {
      classes += ' dev-mode';
    }
    return classes;
  }

  protected computeSourceIndicatorIcon_(): string {
    switch (getItemSource(this.data)) {
      case SourceType.POLICY:
        return 'extensions-icons:business';
      case SourceType.SIDELOADED:
        return 'extensions-icons:input';
      case SourceType.UNKNOWN:
        // TODO(dpapad): Ask UX for a better icon for this case.
        return 'extensions-icons:input';
      case SourceType.UNPACKED:
        return 'extensions-icons:unpacked';
      case SourceType.WEBSTORE:
      case SourceType.INSTALLED_BY_DEFAULT:
        return '';
      default:
        assertNotReached();
    }
  }

  protected computeSourceIndicatorText_(): string {
    if (this.data.locationText) {
      return this.data.locationText;
    }

    const sourceType = getItemSource(this.data);
    return sourceType === SourceType.WEBSTORE ? '' :
                                                getItemSourceString(sourceType);
  }

  protected computeInspectViewsHidden_(): boolean {
    return !this.data.views || this.data.views.length === 0;
  }

  protected computeFirstInspectTitle_(): string {
    // Note: theoretically, this wouldn't be called without any inspectable
    // views (because it's in a dom-if="!computeInspectViewsHidden_()").
    // However, due to the recycling behavior of iron list, it seems that
    // sometimes it can. Even when it is, the UI behaves properly, but we
    // need to handle the case gracefully.
    return this.data.views.length > 0 ?
        computeInspectableViewLabel(this.firstInspectView_) :
        '';
  }

  protected computeFirstInspectLabel_(): string {
    const label = this.computeFirstInspectTitle_();
    return label && this.data.views.length > 1 ? label + ',' : label;
  }

  protected computeExtraViewsHidden_(): boolean {
    return this.data.views.length <= 1;
  }

  protected computeDevReloadButtonHidden_(): boolean {
    return !this.canReloadItem();
  }

  protected computeExtraInspectLabel_(): string {
    return this.i18n(
        'itemInspectViewsExtra', (this.data.views.length - 1).toString());
  }

  /**
   * @return Whether the extension has severe warnings. Doesn't determine the
   *     warning's visibility.
   */
  private hasSevereWarnings_(): boolean {
    return this.data.disableReasons.corruptInstall ||
        this.data.disableReasons.suspiciousInstall ||
        this.data.runtimeWarnings.length > 0 || !!this.data.blocklistText;
  }

  /**
   * @return Whether the extension has an MV2 warning. Doesn't determine the
   *     warning's visibility.
   */
  private hasMv2DeprecationWarning_(): boolean {
    return this.data.disableReasons.unsupportedManifestVersion;
  }

  /**
   * @return Whether the extension has an allowlist warning. Doesn't determine
   *     the warning's visibility.
   */
  private hasAllowlistWarning_(): boolean {
    return this.data.showSafeBrowsingAllowlistWarning;
  }

  protected showDescription_(): boolean {
    // Description is only visible iff no warnings are visible.
    return !this.hasSevereWarnings_() && !this.hasMv2DeprecationWarning_() &&
        !this.hasAllowlistWarning_();
  }

  protected showSevereWarnings(): boolean {
    // Severe warning are always visible, if they exist.
    return this.hasSevereWarnings_();
  }

  protected showMv2DeprecationWarning_(): boolean {
    // MV2 deprecation warning is visible, if existent, if there are no severe
    // warnings visible.
    // Note: The item card has a fixed height and the content might get cropped
    // if too many warnings are displayed.
    return this.hasMv2DeprecationWarning_() && !this.hasSevereWarnings_();
  }

  protected showAllowlistWarning_(): boolean {
    // Allowlist warning is visible, if existent, if there are no severe
    // warnings or mv2 deprecation warnings visible.
    // Note: The item card has a fixed height and the content might get cropped
    // if too many warnings are displayed. This should be a rare edge case and
    // the allowlist warning will still be shown in the item detail view.
    return this.hasAllowlistWarning_() && !this.hasSevereWarnings_() &&
        !this.hasMv2DeprecationWarning_();
  }
}

// Exported to be used in the autogenerated Lit template file
export type ItemElement = ExtensionsItemElement;

declare global {
  interface HTMLElementTagNameMap {
    'extensions-item': ExtensionsItemElement;
  }
}

customElements.define(ExtensionsItemElement.is, ExtensionsItemElement);
