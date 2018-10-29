// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const DetailView = Polymer({
    is: 'extensions-detail-view',

    behaviors: [
      CrContainerShadowBehavior,
      extensions.ItemBehavior,
    ],

    properties: {
      /**
       * The underlying ExtensionInfo for the details being displayed.
       * @type {chrome.developerPrivate.ExtensionInfo}
       */
      data: Object,

      /** @private */
      size_: String,

      /** @type {!extensions.ItemDelegate} */
      delegate: Object,

      /** Whether the user has enabled the UI's developer mode. */
      inDevMode: Boolean,

      /** Whether "allow in incognito" option should be shown. */
      incognitoAvailable: Boolean,

      /** Whether "View Activity Log" link should be shown. */
      showActivityLog: Boolean,
    },

    observers: [
      'onItemIdChanged_(data.id, delegate)',
    ],

    listeners: {
      'view-enter-start': 'onViewEnterStart_',
    },

    /**
     * Focuses the back button when page is loaded.
     * @private
     */
    onViewEnterStart_: function() {
      Polymer.RenderStatus.afterNextRender(
          this, () => cr.ui.focusWithoutInk(this.$.closeButton));
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
      extensions.navigation.navigateTo(
          {page: Page.ACTIVITY_LOG, extensionId: this.data.id});
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
      extensions.navigation.navigateTo({page: Page.LIST});
    },

    /**
     * @return {boolean}
     * @private
     */
    isControlled_: function() {
      return extensions.isControlled(this.data);
    },

    /**
     * @return {boolean}
     * @private
     */
    isEnabled_: function() {
      return extensions.isEnabled(this.data.state);
    },

    /**
     * @return {boolean}
     * @private
     */
    isEnableToggleEnabled_: function() {
      return extensions.userCanChangeEnablement(this.data);
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
          this.data.disableReasons.updateRequired ||
          !!this.data.blacklistText || this.data.runtimeWarnings.length > 0;
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
      return extensions.isEnabled(state) ? onText : offText;
    },

    /**
     * @param {!chrome.developerPrivate.ExtensionView} view
     * @return {string}
     * @private
     */
    computeInspectLabel_: function(view) {
      return extensions.computeInspectableViewLabel(view);
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
      this.delegate.setItemEnabled(
          this.data.id, this.$['enable-toggle'].checked);
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
          extensions.getItemSourceString(extensions.getItemSource(this.data));
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
        case 'CHILD_CUSTODIAN':
          return 'cr:account-child-invert';
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
          !!this.data.permissions.hostAccess;
    },

    /**
     * @return {boolean}
     * @private
     */
    showRuntimeHostPermissions_: function() {
      return !!this.data.permissions.hostAccess;
    },
  });

  return {DetailView: DetailView};
});
