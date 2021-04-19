// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'protocol-handlers' is the polymer element for showing the
 * protocol handlers category under Site Settings.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import '../privacy_page/collapse_radio_button.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {SiteSettingsBehavior} from './site_settings_behavior.js';

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
export let HandlerEntry;

/**
 * @typedef {{handlers: !Array<!HandlerEntry>,
 *            protocol: string,
 *            protocol_display_name: string}}
 */
export let ProtocolEntry;

Polymer({
  is: 'protocol-handlers',

  _template: html`{__html_template__}`,

  behaviors: [
    SiteSettingsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
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

    /** @private */
    enableContentSettingsRedesign_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableContentSettingsRedesign');
      }
    },

    /** @private {chrome.settingsPrivate.PrefObject} */
    handlersEnabledPref_: {
      type: Object,
      value() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        });
      },
    },
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'setHandlersEnabled', this.setHandlersEnabled_.bind(this));
    this.addWebUIListener(
        'setProtocolHandlers', this.setProtocolHandlers_.bind(this));
    this.addWebUIListener(
        'setIgnoredProtocolHandlers',
        this.setIgnoredProtocolHandlers_.bind(this));
    this.browserProxy.observeProtocolHandlers();
  },

  /**
   * Obtains the description for the main toggle.
   * @return {string} The description to use.
   * @private
   */
  computeHandlersDescription_() {
    return this.handlersEnabledPref_.value ? this.toggleOnLabel :
                                             this.toggleOffLabel;
  },

  /**
   * Updates the main toggle to set it enabled/disabled.
   * @param {boolean} enabled The state to set.
   * @private
   */
  setHandlersEnabled_(enabled) {
    this.set('handlersEnabledPref_.value', enabled);
  },

  /**
   * Updates the list of protocol handlers.
   * @param {!Array<!ProtocolEntry>} protocols The new protocol handler list.
   * @private
   */
  setProtocolHandlers_(protocols) {
    this.protocols = protocols;
  },

  /**
   * Updates the list of ignored protocol handlers.
   * @param {!Array<!HandlerEntry>} ignoredProtocols The new (ignored) protocol
   *     handler list.
   * @private
   */
  setIgnoredProtocolHandlers_(ignoredProtocols) {
    this.ignoredProtocols = ignoredProtocols;
  },

  /**
   * Closes action menu and resets action menu model
   * @private
   */
  closeActionMenu_() {
    this.$$('cr-action-menu').close();
    this.actionMenuModel_ = null;
  },

  /**
   * A handler when the toggle is flipped.
   * @private
   */
  onToggleChange_() {
    this.browserProxy.setProtocolHandlerDefault(
        !!this.handlersEnabledPref_.value);
  },

  /**
   * The handler for when "Set Default" is selected in the action menu.
   * @private
   */
  onDefaultClick_() {
    const item = this.actionMenuModel_;
    this.browserProxy.setProtocolDefault(item.protocol, item.spec);
    this.closeActionMenu_();
  },

  /**
   * The handler for when "Remove" is selected in the action menu.
   * @private
   */
  onRemoveClick_() {
    const item = this.actionMenuModel_;
    this.browserProxy.removeProtocolHandler(item.protocol, item.spec);
    this.closeActionMenu_();
  },

  /**
   * Handler for removing handlers that were blocked
   * @private
   */
  onRemoveIgnored_(event) {
    const item = event.model.item;
    this.browserProxy.removeProtocolHandler(item.protocol, item.spec);
  },

  /**
   * A handler to show the action menu next to the clicked menu button.
   * @param {!{model: !{item: HandlerEntry}}} event
   * @private
   */
  showMenu_(event) {
    this.actionMenuModel_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(
            /** @type {!Element} */ (/** @type {!Event} */ (event).target));
  }
});
