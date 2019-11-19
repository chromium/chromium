// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
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
import {computeInspectableViewLabel, getItemSource, getItemSourceString, isControlled, isEnabled, userCanChangeEnablement} from './item_util.js';
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
  focusOptionsButton: function() {
    this.$$('#extensions-options').focus();
  },

  /**
   * Focuses the back button when page is loaded.
   * @private
   */
  onViewEnterStart_: function() {
    const elementToFocus = this.fromActivityLog ?
        this.$.extensionsActivityLogLink :
        this.$.closeButton;

    afterNextRender(this, () => focusWithoutInk(elementToFocus));
  },

  /** @private */
  onItemIdChanged_: function() {
    // Clear the size, since this view is reused, such that no obsolete size
    // is displayed.:
    this.size_ = '';
    this.delegate.getExtensionSize(this.data.id).then(size => {
      this.size_ = size;
    });
  },

  /** @private */
  onActivityLogTap_: function() {
    navigation.navigateTo({page: Page.ACTIVITY_LOG, extensionId: this.data.id});
  },

  /**
   * @param {string} description
   * @param {string} fallback
   * @return {string}
   * @private
   */
  getDescription_: function(description, fallback) {
    return description || fallback;
  },

  /** @private */
  onCloseButtonTap_: function() {
    navigation.navigateTo({page: Page.LIST});
  },

  /**
   * @return {boolean}
   * @private
   */
  isControlled_: function() {
    return isControlled(this.data);
  },

  /**
   * @return {boolean}
   * @private
   */
  isEnabled_: function() {
    return isEnabled(this.data.state);
  },

  /**
   * @return {boolean}
   * @private
   */
  isEnableToggleEnabled_: function() {
    return userCanChangeEnablement(this.data);
  },

  /**
   * Returns true if the extension is in the terminated state.
   * @return {boolean}
   * @private
   */
  isTerminated_: function() {
    return this.data.state == chrome.developerPrivate.ExtensionState.TERMINATED;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasDependentExtensions_: function() {
    return this.data.dependentExtensions.length > 0;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasWarnings_: function() {
    return this.data.disableReasons.corruptInstall ||
        this.data.disableReasons.suspiciousInstall ||
        this.data.disableReasons.updateRequired || !!this.data.blacklistText ||
        this.data.runtimeWarnings.length > 0;
  },

  /**
   * @return {string}
   * @private
   */
  computeEnabledStyle_: function() {
    return this.isEnabled_() ? 'enabled-text' : '';
  },

  /**
   * @param {!chrome.developerPrivate.ExtensionState} state
   * @param {string} onText
   * @param {string} offText
   * @return {string}
   * @private
   */
  computeEnabledText_: function(state, onText, offText) {
    // TODO(devlin): Get the full spectrum of these strings from bettes.
    return isEnabled(state) ? onText : offText;
  },

  /**
   * @param {!chrome.developerPrivate.ExtensionView} view
   * @return {string}
   * @private
   */
  computeInspectLabel_: function(view) {
    return computeInspectableViewLabel(view);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowOptionsLink_: function() {
    return !!this.data.optionsPage;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowOptionsSection_: function() {
    return this.data.incognitoAccess.isEnabled ||
        this.data.fileAccess.isEnabled || this.data.errorCollection.isEnabled;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowIncognitoOption_: function() {
    return this.data.incognitoAccess.isEnabled && this.incognitoAvailable;
  },

  /** @private */
  onEnableChange_: function() {
    this.delegate.setItemEnabled(this.data.id, this.$['enable-toggle'].checked);
  },

  /**
   * @param {!{model: !{item: !chrome.developerPrivate.ExtensionView}}} e
   * @private
   */
  onInspectTap_: function(e) {
    this.delegate.inspectItemView(this.data.id, e.model.item);
  },

  /** @private */
  onExtensionOptionsTap_: function() {
    this.delegate.showItemOptionsPage(this.data);
  },

  /** @private */
  onReloadTap_: function() {
    this.delegate.reloadItem(this.data.id).catch(loadError => {
      this.fire('load-error', loadError);
    });
  },

  /** @private */
  onRemoveTap_: function() {
    this.delegate.deleteItem(this.data.id);
  },

  /** @private */
  onRepairTap_: function() {
    this.delegate.repairItem(this.data.id);
  },

  /** @private */
  onLoadPathTap_: function() {
    this.delegate.showInFolder(this.data.id);
  },

  /** @private */
  onAllowIncognitoChange_: function() {
    this.delegate.setItemAllowedIncognito(
        this.data.id, this.$$('#allow-incognito').checked);
  },

  /** @private */
  onAllowOnFileUrlsChange_: function() {
    this.delegate.setItemAllowedOnFileUrls(
        this.data.id, this.$$('#allow-on-file-urls').checked);
  },

  /** @private */
  onCollectErrorsChange_: function() {
    this.delegate.setItemCollectsErrors(
        this.data.id, this.$$('#collect-errors').checked);
  },

  /** @private */
  onExtensionWebSiteTap_: function() {
    this.delegate.openUrl(this.data.manifestHomePageUrl);
  },

  /** @private */
  onViewInStoreTap_: function() {
    this.delegate.openUrl(this.data.webStoreUrl);
  },

  /**
   * @param {!chrome.developerPrivate.DependentExtension} item
   * @return {string}
   * @private
   */
  computeDependentEntry_: function(item) {
    return loadTimeData.getStringF('itemDependentEntry', item.name, item.id);
  },

  /**
   * @return {string}
   * @private
   */
  computeSourceString_: function() {
    return this.data.locationText ||
        getItemSourceString(getItemSource(this.data));
  },

  /**
   * @param {chrome.developerPrivate.ControllerType} type
   * @return {string}
   * @private
   */
  getIndicatorIcon_: function(type) {
    switch (type) {
      case 'POLICY':
        return 'cr20:domain';
      case 'SUPERVISED_USER_CUSTODIAN':
        return 'cr:supervisor-account';
      default:
        return '';
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  hasPermissions_: function() {
    return this.data.permissions.simplePermissions.length > 0 ||
        this.hasRuntimeHostPermissions_();
  },

  /**
   * @return {boolean}
   * @private
   */
  hasRuntimeHostPermissions_: function() {
    return !!this.data.permissions.runtimeHostPermissions;
  },

  /**
   * @return {boolean}
   * @private
   */
  showSiteAccessContent_: function() {
    return this.showFreeformRuntimeHostPermissions_() ||
        this.showHostPermissionsToggleList_();
  },

  /**
   * @return {boolean}
   * @private
   */
  showFreeformRuntimeHostPermissions_: function() {
    return this.hasRuntimeHostPermissions_() &&
        this.data.permissions.runtimeHostPermissions.hasAllHosts;
  },

  /**
   * @return {boolean}
   * @private
   */
  showHostPermissionsToggleList_: function() {
    return this.hasRuntimeHostPermissions_() &&
        !this.data.permissions.runtimeHostPermissions.hasAllHosts;
  },
});
