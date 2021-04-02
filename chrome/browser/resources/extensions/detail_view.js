// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './host_permissions_toggle_list.js';
import './runtime_host_permissions.js';
import './shared_style.js';
import './shared_vars.js';
import './strings.m.js';
import './toggle_row.js';

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';
import {ItemBehavior} from './item_behavior.js';
import {computeInspectableViewLabel, EnableControl, getEnableControl, getItemSource, getItemSourceString, isEnabled, userCanChangeEnablement} from './item_util.js';
import {navigation, Page} from './navigation_helper.js';

Polymer({
  is: 'extensions-detail-view',

  _template: html`{__html_template__}`,

  behaviors: [
    CrContainerShadowBehavior,
    ItemBehavior,
  ],

  properties: {
    /**
     * The underlying ExtensionInfo for the details being displayed.
     * @type {!chrome.developerPrivate.ExtensionInfo}
     */
    data: Object,

    /** @private */
    size_: String,

    /** @type {!ItemDelegate} */
    delegate: Object,

    /** Whether the user has enabled the UI's developer mode. */
    inDevMode: Boolean,

    /** Whether "allow in incognito" option should be shown. */
    incognitoAvailable: Boolean,

    /** Whether "View Activity Log" link should be shown. */
    showActivityLog: Boolean,

    /** Whether the user navigated to this page from the activity log page. */
    fromActivityLog: Boolean,
  },

  observers: [
    'onItemIdChanged_(data.id, delegate)',
  ],

  listeners: {
    'view-enter-start': 'onViewEnterStart_',
  },

  /**
   * Focuses the extensions options button. This should be used after the
   * dialog closes.
   */
  focusOptionsButton() {
    this.$$('#extensions-options').focus();
  },

  /**
   * Focuses the back button when page is loaded.
   * @private
   */
  onViewEnterStart_() {
    const elementToFocus = this.fromActivityLog ?
        this.$.extensionsActivityLogLink :
        this.$.closeButton;

    afterNextRender(this, () => focusWithoutInk(elementToFocus));
  },

  /** @private */
  onItemIdChanged_() {
    // Clear the size, since this view is reused, such that no obsolete size
    // is displayed.:
    this.size_ = '';
    this.delegate.getExtensionSize(this.data.id).then(size => {
      this.size_ = size;
    });
  },

  /** @private */
  onActivityLogTap_() {
    navigation.navigateTo({page: Page.ACTIVITY_LOG, extensionId: this.data.id});
  },

  /**
   * @param {string} description
   * @param {string} fallback
   * @return {string}
   * @private
   */
  getDescription_(description, fallback) {
    return description || fallback;
  },

  /** @private */
  onCloseButtonTap_() {
    navigation.navigateTo({page: Page.LIST});
  },

  /**
   * @return {boolean}
   * @private
   */
  isEnabled_() {
    return isEnabled(this.data.state);
  },

  /**
   * @return {boolean}
   * @private
   */
  isEnableToggleEnabled_() {
    return userCanChangeEnablement(this.data);
  },

  /**
   * @return {boolean}
   * @private
   */
  hasDependentExtensions_() {
    return this.data.dependentExtensions.length > 0;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasSevereWarnings_() {
    return this.data.disableReasons.corruptInstall ||
        this.data.disableReasons.suspiciousInstall ||
        this.data.disableReasons.updateRequired || !!this.data.blacklistText ||
        this.data.runtimeWarnings.length > 0;
  },

  /**
   * @return {string}
   * @private
   */
  computeEnabledStyle_() {
    return this.isEnabled_() ? 'enabled-text' : '';
  },

  /**
   * @param {!chrome.developerPrivate.ExtensionState} state
   * @param {string} onText
   * @param {string} offText
   * @return {string}
   * @private
   */
  computeEnabledText_(state, onText, offText) {
    // TODO(devlin): Get the full spectrum of these strings from bettes.
    return isEnabled(state) ? onText : offText;
  },

  /**
   * @param {!chrome.developerPrivate.ExtensionView} view
   * @return {string}
   * @private
   */
  computeInspectLabel_(view) {
    return computeInspectableViewLabel(view);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowOptionsLink_() {
    return !!this.data.optionsPage;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowOptionsSection_() {
    return this.data.incognitoAccess.isEnabled ||
        this.data.fileAccess.isEnabled || this.data.errorCollection.isEnabled;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowIncognitoOption_() {
    return this.data.incognitoAccess.isEnabled && this.incognitoAvailable;
  },

  /** @private */
  onEnableToggleChange_() {
    this.delegate.setItemEnabled(this.data.id, this.$.enableToggle.checked);
    this.$.enableToggle.checked = this.isEnabled_();
  },

  /**
   * @param {!{model: !{item: !chrome.developerPrivate.ExtensionView}}} e
   * @private
   */
  onInspectTap_(e) {
    this.delegate.inspectItemView(this.data.id, e.model.item);
  },

  /** @private */
  onExtensionOptionsTap_() {
    this.delegate.showItemOptionsPage(this.data);
  },

  /** @private */
  onReloadTap_() {
    this.delegate.reloadItem(this.data.id).catch(loadError => {
      this.fire('load-error', loadError);
    });
  },

  /** @private */
  onRemoveTap_() {
    this.delegate.deleteItem(this.data.id);
  },

  /** @private */
  onRepairTap_() {
    this.delegate.repairItem(this.data.id);
  },

  /** @private */
  onLoadPathTap_() {
    this.delegate.showInFolder(this.data.id);
  },

  /** @private */
  onAllowIncognitoChange_() {
    this.delegate.setItemAllowedIncognito(
        this.data.id, this.$$('#allow-incognito').checked);
  },

  /** @private */
  onAllowOnFileUrlsChange_() {
    this.delegate.setItemAllowedOnFileUrls(
        this.data.id, this.$$('#allow-on-file-urls').checked);
  },

  /** @private */
  onCollectErrorsChange_() {
    this.delegate.setItemCollectsErrors(
        this.data.id, this.$$('#collect-errors').checked);
  },

  /** @private */
  onExtensionWebSiteTap_() {
    this.delegate.openUrl(this.data.manifestHomePageUrl);
  },

  /** @private */
  onViewInStoreTap_() {
    this.delegate.openUrl(this.data.webStoreUrl);
  },

  /**
   * @param {!chrome.developerPrivate.DependentExtension} item
   * @return {string}
   * @private
   */
  computeDependentEntry_(item) {
    return loadTimeData.getStringF('itemDependentEntry', item.name, item.id);
  },

  /**
   * @return {string}
   * @private
   */
  computeSourceString_() {
    return this.data.locationText ||
        getItemSourceString(getItemSource(this.data));
  },

  /**
   * @return {boolean}
   * @private
   */
  hasPermissions_() {
    return this.data.permissions.simplePermissions.length > 0 ||
        this.hasRuntimeHostPermissions_();
  },

  /**
   * @return {boolean}
   * @private
   */
  hasRuntimeHostPermissions_() {
    return !!this.data.permissions.runtimeHostPermissions;
  },

  /**
   * @return {boolean}
   * @private
   */
  showSiteAccessContent_() {
    return this.showFreeformRuntimeHostPermissions_() ||
        this.showHostPermissionsToggleList_();
  },

  /**
   * @return {boolean}
   * @private
   */
  showFreeformRuntimeHostPermissions_() {
    return this.hasRuntimeHostPermissions_() &&
        this.data.permissions.runtimeHostPermissions.hasAllHosts;
  },

  /**
   * @return {boolean}
   * @private
   */
  showHostPermissionsToggleList_() {
    return this.hasRuntimeHostPermissions_() &&
        !this.data.permissions.runtimeHostPermissions.hasAllHosts;
  },

  /**
   * Returns true if the reload button should be shown.
   * @return {boolean}
   * @private
   */
  showReloadButton_() {
    return getEnableControl(this.data) === EnableControl.RELOAD;
  },

  /**
   * Returns true if the repair button should be shown.
   * @return {boolean}
   * @private
   */
  showRepairButton_() {
    return getEnableControl(this.data) === EnableControl.REPAIR;
  },

  /**
   * Returns true if the enable toggle should be shown.
   * @return {boolean}
   * @private
   */
  showEnableToggle_() {
    const enableControl = getEnableControl(this.data);
    // We still show the toggle even if we also show the repair button in the
    // detail view, because the repair button appears just beneath it.
    return enableControl === EnableControl.ENABLE_TOGGLE ||
        enableControl === EnableControl.REPAIR;
  },

  /**
   * @return {boolean} Whether the allowlist warning should be shown.
   * @private
   */
  showAllowlistWarning_() {
    // Only show the allowlist warning if there is no blocklist warning. It
    // would be redundant since all blocklisted items are necessarily not
    // included in the Safe Browsing allowlist.
    return this.data.showSafeBrowsingAllowlistWarning &&
        !this.data.blacklistText;
  },
});
