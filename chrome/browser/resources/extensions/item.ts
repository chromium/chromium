// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link.css.js';
import './icons.html.js';
import './shared_style.css.js';
import './shared_vars.css.js';
import './strings.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './item.html.js';
import {ItemMixin} from './item_mixin.js';
import {computeInspectableViewLabel, EnableControl, getEnableControl, getEnableToggleAriaLabel, getItemSource, getItemSourceString, isEnabled, sortViews, SourceType, userCanChangeEnablement} from './item_util.js';
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
  setItemSafetyCheckWarningAcknowledged(id: string): void;
  setShowAccessRequestsInToolbar(id: string, showRequests: boolean): void;

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

const ExtensionsItemElementBase = I18nMixin(ItemMixin(PolymerElement));

export class ExtensionsItemElement extends ExtensionsItemElementBase {
  static get is() {
    return 'extensions-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // The item's delegate, or null.
      delegate: Object,

      // Whether or not dev mode is enabled.
      inDevMode: {
        type: Boolean,
        value: false,
      },

      safetyCheckShowing: {
        type: Boolean,
        value: false,
      },

      // The underlying ExtensionInfo itself. Public for use in declarative
      // bindings.
      data: Object,

      // Whether or not the expanded view of the item is shown.
      showingDetails_: {
        type: Boolean,
        value: false,
      },

      // First inspectable view after sorting.
      firstInspectView_: {
        type: Object,
        computed: 'computeFirstInspectView_(data.views)',
      },
    };
  }

  static get observers() {
    return ['observeIdVisibility_(inDevMode, showingDetails_, data.id)'];
  }

  delegate: ItemDelegate;
  inDevMode: boolean;
  safetyCheckShowing: boolean;
  data: chrome.developerPrivate.ExtensionInfo;
  private showingDetails_: boolean;
  private firstInspectView_: chrome.developerPrivate.ExtensionView;
  /** Prevents reloading the same item while it's already being reloaded. */
  private isReloading_: boolean = false;

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  getDetailsButton() {
    return this.$.detailsButton;
  }

  /** @return The "Errors" button, if it exists. */
  getErrorsButton(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#errors-button');
  }

  private getEnableToggleAriaLabel_(): string {
    return getEnableToggleAriaLabel(
        this.isEnabled_(), this.data.type, this.i18n('appEnabled'),
        this.i18n('extensionEnabled'), this.i18n('itemOff'));
  }

  private observeIdVisibility_() {
    flush();
    const idElement = this.shadowRoot!.querySelector('#extension-id');
    if (idElement) {
      assert(this.data);
      idElement.textContent = this.i18n('itemId', this.data.id);
    }
  }

  private shouldShowErrorsButton_(): boolean {
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

  private onRemoveClick_() {
    if (this.safetyCheckShowing) {
      const actionToRecord = this.data.safetyCheckText ?
          'SafetyCheck.ReviewPanelRemoveClicked' :
          'SafetyCheck.NonTriggeringExtensionRemoved';
      chrome.metricsPrivate.recordUserAction(actionToRecord);
    }
    this.delegate.deleteItem(this.data.id);
  }

  private onEnableToggleChange_() {
    this.delegate.setItemEnabled(this.data.id, this.$.enableToggle.checked);
    this.$.enableToggle.checked = this.isEnabled_();
  }

  private onErrorsClick_() {
    if (this.data.installWarnings && this.data.installWarnings.length > 0) {
      this.fire_('show-install-warnings', this.data.installWarnings);
      return;
    }

    navigation.navigateTo({page: Page.ERRORS, extensionId: this.data.id});
  }

  private onDetailsClick_() {
    navigation.navigateTo({page: Page.DETAILS, extensionId: this.data.id});
  }

  private computeFirstInspectView_(): chrome.developerPrivate.ExtensionView {
    return sortViews(this.data.views)[0];
  }

  private onInspectClick_() {
    this.delegate.inspectItemView(this.data.id, this.firstInspectView_);
  }

  private onExtraInspectClick_() {
    navigation.navigateTo({page: Page.DETAILS, extensionId: this.data.id});
  }

  private onReloadClick_() {
    // Don't reload if in the middle of an update.
    if (this.isReloading_) {
      return;
    }

    this.isReloading_ = true;

    const toastManager = getToastManager();
    // Keep the toast open indefinitely.
    toastManager.duration = 0;
    toastManager.show(this.i18n('itemReloading'));
    this.delegate.reloadItem(this.data.id)
        .then(
            () => {
              toastManager.hide();
              toastManager.duration = 3000;
              toastManager.show(this.i18n('itemReloaded'));
              this.isReloading_ = false;
            },
            loadError => {
              this.fire_('load-error', loadError);
              toastManager.hide();
              this.isReloading_ = false;
            });
  }

  private onRepairClick_() {
    this.delegate.repairItem(this.data.id);
  }

  private isEnabled_(): boolean {
    return isEnabled(this.data.state);
  }

  private isEnableToggleEnabled_(): boolean {
    return userCanChangeEnablement(this.data);
  }

  /** @return Whether the reload button should be shown. */
  private showReloadButton_(): boolean {
    return getEnableControl(this.data) === EnableControl.RELOAD;
  }

  /** @return Whether the repair button should be shown. */
  private showRepairButton_(): boolean {
    return getEnableControl(this.data) === EnableControl.REPAIR;
  }


  /** @return Whether the enable toggle should be shown. */
  private showEnableToggle_(): boolean {
    return getEnableControl(this.data) === EnableControl.ENABLE_TOGGLE;
  }

  private computeClasses_(): string {
    let classes = this.isEnabled_() ? 'enabled' : 'disabled';
    if (this.inDevMode) {
      classes += ' dev-mode';
    }
    return classes;
  }

  private computeSourceIndicatorIcon_(): string {
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

  private computeSourceIndicatorText_(): string {
    if (this.data.locationText) {
      return this.data.locationText;
    }

    const sourceType = getItemSource(this.data);
    return sourceType === SourceType.WEBSTORE ? '' :
                                                getItemSourceString(sourceType);
  }

  private computeInspectViewsHidden_(): boolean {
    return !this.data.views || this.data.views.length === 0;
  }

  private computeFirstInspectTitle_(): string {
    // Note: theoretically, this wouldn't be called without any inspectable
    // views (because it's in a dom-if="!computeInspectViewsHidden_()").
    // However, due to the recycling behavior of iron list, it seems that
    // sometimes it can. Even when it is, the UI behaves properly, but we
    // need to handle the case gracefully.
    return this.data.views.length > 0 ?
        computeInspectableViewLabel(this.firstInspectView_) :
        '';
  }

  private computeFirstInspectLabel_(): string {
    const label = this.computeFirstInspectTitle_();
    return label && this.data.views.length > 1 ? label + ',' : label;
  }

  private computeExtraViewsHidden_(): boolean {
    return this.data.views.length <= 1;
  }

  private computeDevReloadButtonHidden_(): boolean {
    // Only display the reload spinner if the extension is unpacked and
    // enabled or disabled for reload. If an extension fails to reload (due to
    // e.g. a parsing error), it will
    // remain disabled with the "reloading" reason. We show the reload button
    // when it's disabled for reload to enable developers to reload the fixed
    // version. (Note that trying to reload an extension that is currently
    // trying to reload is a no-op.) For other
    // disableReasons, there's no point in reloading a disabled extension, and
    // we'll show a crashed reload button if it's terminated.
    const showIcon =
        this.data.location === chrome.developerPrivate.Location.UNPACKED &&
        (this.data.state === chrome.developerPrivate.ExtensionState.ENABLED ||
         this.data.disableReasons.reloading);
    return !showIcon;
  }

  private computeExtraInspectLabel_(): string {
    return this.i18n(
        'itemInspectViewsExtra', (this.data.views.length - 1).toString());
  }

  private hasSevereWarnings_(): boolean {
    return this.data.disableReasons.corruptInstall ||
        this.data.disableReasons.suspiciousInstall ||
        this.data.runtimeWarnings.length > 0 || !!this.data.blacklistText;
  }

  private showDescription_(): boolean {
    return !this.hasSevereWarnings_() &&
        !this.data.showSafeBrowsingAllowlistWarning;
  }

  private showAllowlistWarning_(): boolean {
    // Only show the allowlist warning if there are no other warnings. The item
    // card has a fixed height and the content might get cropped if too many
    // warnings are displayed. This should be a rare edge case and the allowlist
    // warning will still be shown in the item detail view.
    return this.data.showSafeBrowsingAllowlistWarning &&
        !this.hasSevereWarnings_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-item': ExtensionsItemElement;
  }
}

customElements.define(ExtensionsItemElement.is, ExtensionsItemElement);
