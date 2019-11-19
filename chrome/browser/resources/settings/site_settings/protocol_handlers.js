// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'protocol-handlers' is the polymer element for showing the
 * protocol handlers category under Site Settings.
 */

/**
 * All possible actions in the menu.
 * @enum {string}
 */
const MenuActions = {
  SET_DEFAULT: 'SetDefault',
  REMOVE: 'Remove',
};

/**
 * @typedef {{host: string,
 *            is_default: boolean,
 *            protocol: string,
 *            protocol_display_name: string,
 *            spec: string}}
 */
let HandlerEntry;

/**
 * @typedef {{handlers: !Array<!HandlerEntry>,
 *            protocol: string,
 *            protocol_display_name: string}}
 */
let ProtocolEntry;

Polymer({
  is: 'protocol-handlers',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Represents the state of the main toggle shown for the category.
     */
    categoryEnabled: Boolean,

    /**
     * Array of protocols and their handlers.
     * @type {!Array<!ProtocolEntry>}
     */
    protocols: Array,

    /**
     * The targetted object for menu operations.
     * @private {?HandlerEntry}
     */
    actionMenuModel_: Object,

    /* Labels for the toggle on/off positions. */
    toggleOffLabel: String,
    toggleOnLabel: String,

    /**
     * Array of ignored (blocked) protocols.
     * @type {!Array<!HandlerEntry>}
     */
    ignoredProtocols: Array,

    // <if expr="chromeos">
    /** @private */
    settingsAppAvailable_: {
      type: Boolean,
      value: false,
    },
    // </if>
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'setHandlersEnabled', this.setHandlersEnabled_.bind(this));
    this.addWebUIListener(
        'setProtocolHandlers', this.setProtocolHandlers_.bind(this));
    this.addWebUIListener(
        'setIgnoredProtocolHandlers',
        this.setIgnoredProtocolHandlers_.bind(this));
    this.browserProxy.observeProtocolHandlers();
  },

  // <if expr="chromeos">
  /** @override */
  attached: function() {
    if (settings.AndroidAppsBrowserProxyImpl) {
      cr.addWebUIListener(
          'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
      settings.AndroidAppsBrowserProxyImpl.getInstance()
          .requestAndroidAppsInfo();
    }
  },
  // </if>

  // <if expr="chromeos">
  /**
   * Receives updates on whether or not ARC settings app is available.
   * @param {AndroidAppsInfo} info
   * @private
   */
  androidAppsInfoUpdate_: function(info) {
    this.settingsAppAvailable_ = info.settingsAppAvailable;
  },
  // </if>

  /** @private */
  categoryLabelClicked_: function() {
    this.$.toggle.click();
  },

  /**
   * Obtains the description for the main toggle.
   * @return {string} The description to use.
   * @private
   */
  computeHandlersDescription_: function() {
    return this.categoryEnabled ? this.toggleOnLabel : this.toggleOffLabel;
  },

  /**
   * Updates the main toggle to set it enabled/disabled.
   * @param {boolean} enabled The state to set.
   * @private
   */
  setHandlersEnabled_: function(enabled) {
    this.categoryEnabled = enabled;
  },

  /**
   * Updates the list of protocol handlers.
   * @param {!Array<!ProtocolEntry>} protocols The new protocol handler list.
   * @private
   */
  setProtocolHandlers_: function(protocols) {
    this.protocols = protocols;
  },

  /**
   * Updates the list of ignored protocol handlers.
   * @param {!Array<!HandlerEntry>} ignoredProtocols The new (ignored) protocol
   *     handler list.
   * @private
   */
  setIgnoredProtocolHandlers_: function(ignoredProtocols) {
    this.ignoredProtocols = ignoredProtocols;
  },

  /**
   * Closes action menu and resets action menu model
   * @private
   */
  closeActionMenu_: function() {
    this.$$('cr-action-menu').close();
    this.actionMenuModel_ = null;
  },

  /**
   * A handler when the toggle is flipped.
   * @private
   */
  onToggleChange_: function(event) {
    this.browserProxy.setProtocolHandlerDefault(this.categoryEnabled);
  },

  /**
   * The handler for when "Set Default" is selected in the action menu.
   * @private
   */
  onDefaultClick_: function() {
    const item = this.actionMenuModel_;
    this.browserProxy.setProtocolDefault(item.protocol, item.spec);
    this.closeActionMenu_();
  },

  /**
   * The handler for when "Remove" is selected in the action menu.
   * @private
   */
  onRemoveClick_: function() {
    const item = this.actionMenuModel_;
    this.browserProxy.removeProtocolHandler(item.protocol, item.spec);
    this.closeActionMenu_();
  },

  /**
   * Handler for removing handlers that were blocked
   * @private
   */
  onRemoveIgnored_: function(event) {
    const item = event.model.item;
    this.browserProxy.removeProtocolHandler(item.protocol, item.spec);
  },

  /**
   * A handler to show the action menu next to the clicked menu button.
   * @param {!{model: !{item: HandlerEntry}}} event
   * @private
   */
  showMenu_: function(event) {
    this.actionMenuModel_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(
            /** @type {!Element} */ (/** @type {!Event} */ (event).target));
  },

  // <if expr="chromeos">
  /**
   * Opens an activity to handle App links (preferred apps).
   * @private
   */
  onManageAndroidAppsClick_: function() {
    this.browserProxy.showAndroidManageAppLinks();
  },
  // </if>
});
