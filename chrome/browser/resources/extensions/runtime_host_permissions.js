// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './runtime_hosts_dialog.js';
import './shared_style.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';

Polymer({
  is: 'extensions-runtime-host-permissions',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * The underlying permissions data.
     * @type {chrome.developerPrivate.RuntimeHostPermissions}
     */
    permissions: Object,

    /** @private */
    itemId: String,

    /** @type {!ItemDelegate} */
    delegate: Object,

    /**
     * Whether the dialog to add a new host permission is shown.
     * @private
     */
    showHostDialog_: Boolean,

    /**
     * The current site of the entry that the host dialog is editing, if the
     * dialog is open for editing.
     * @type {?string}
     * @private
     */
    hostDialogModel_: {
      type: String,
      value: null,
    },

    /**
     * The element to return focus to once the host dialog closes.
     * @type {?HTMLElement}
     * @private
     */
    hostDialogAnchorElement_: {
      type: Object,
      value: null,
    },

    /**
     * If the action menu is open, the site of the entry it is open for.
     * Otherwise null.
     * @type {?string}
     * @private
     */
    actionMenuModel_: {
      type: String,
      value: null,
    },

    /**
     * The element that triggered the action menu, so that the page will
     * return focus once the action menu (or dialog) closes.
     * @type {?HTMLElement}
     * @private
     */
    actionMenuAnchorElement_: {
      type: Object,
      value: null,
    },

    /**
     * The old host access setting; used when we don't immediately commit the
     * change to host access so that we can reset it if the user cancels.
     * @type {?string}
     * @private
     */
    oldHostAccess_: {
      type: String,
      value: null,
    },

    /**
     * Proxying the enum to be used easily by the html template.
     * @private
     */
    HostAccess_: {
      type: Object,
      value: chrome.developerPrivate.HostAccess,
    },
  },

  /**
   * @param {!Event} event
   * @private
   */
  onHostAccessChange_: function(event) {
    const group = /** @type {!HTMLElement} */ (this.$['host-access']);
    const access = group.selected;

    if (access == chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES &&
        this.permissions.hostAccess !=
            chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES) {
      // If the user is transitioning to the "on specific sites" option, show
      // the "add host" dialog. This serves two purposes:
      // - The user is prompted to add a host immediately, since otherwise
      //   "on specific sites" is meaningless, and
      // - The way the C++ code differentiates between "on click" and "on
      //   specific sites" is by checking if there are any specific sites.
      //   This ensures there will be at least one, so that the host access
      //   is properly calculated.
      this.oldHostAccess_ = this.permissions.hostAccess;
      this.doShowHostDialog_(group, null);
    } else {
      this.delegate.setItemHostAccess(this.itemId, access);
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  showSpecificSites_: function() {
    return this.permissions.hostAccess ==
        chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES;
  },

  /**
   * Returns the granted host permissions as a sorted set of strings.
   * @return {!Array<string>}
   * @private
   */
  getRuntimeHosts_: function() {
    if (!this.permissions.hosts) {
      return [];
    }

    // Only show granted hosts in the list.
    // TODO(devlin): For extensions that request a finite set of hosts,
    // display them in a toggle list. https://crbug.com/891803.
    return this.permissions.hosts.filter(control => control.granted)
        .map(control => control.host)
        .sort();
  },

  /**
   * @param {Event} e
   * @private
   */
  onAddHostClick_: function(e) {
    const target = /** @type {!HTMLElement} */ (e.target);
    this.doShowHostDialog_(target, null);
  },

  /**
   * @param {!HTMLElement} anchorElement The element to return focus to once
   *     the dialog closes.
   * @param {?string} currentSite The site entry currently being
   *     edited, or null if this is to add a new entry.
   * @private
   */
  doShowHostDialog_: function(anchorElement, currentSite) {
    this.hostDialogAnchorElement_ = anchorElement;
    this.hostDialogModel_ = currentSite;
    this.showHostDialog_ = true;
  },

  /** @private */
  onHostDialogClose_: function() {
    this.hostDialogModel_ = null;
    this.showHostDialog_ = false;
    focusWithoutInk(assert(this.hostDialogAnchorElement_, 'Host Anchor'));
    this.hostDialogAnchorElement_ = null;
    this.oldHostAccess_ = null;
  },

  /** @private */
  onHostDialogCancel_: function() {
    // The user canceled the dialog. Set host-access back to the old value,
    // if the dialog was shown when just transitioning to a new state.
    if (this.oldHostAccess_) {
      assert(this.permissions.hostAccess == this.oldHostAccess_);
      this.$['host-access'].selected = this.oldHostAccess_;
      this.oldHostAccess_ = null;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  dialogShouldUpdateHostAccess_: function() {
    return !!this.oldHostAccess_;
  },

  /**
   * @param {!{
   *   model: !{item: string},
   *   target: !HTMLElement,
   * }} e
   * @private
   */
  onEditHostClick_: function(e) {
    this.actionMenuModel_ = e.model.item;
    this.actionMenuAnchorElement_ = e.target;
    const actionMenu =
        /** @type {CrActionMenuElement} */ (this.$.hostActionMenu);
    actionMenu.showAt(e.target);
  },

  /** @private */
  onActionMenuEditClick_: function() {
    // Cache the site before closing the action menu, since it's cleared.
    const site = this.actionMenuModel_;

    // Cache and reset actionMenuAnchorElement_ so focus is not returned
    // to the action menu's trigger (since the dialog will be shown next).
    // Instead, curry the element to the dialog, so once it closes, focus
    // will be returned.
    const anchorElement = assert(this.actionMenuAnchorElement_, 'Menu Anchor');
    this.actionMenuAnchorElement_ = null;
    this.closeActionMenu_();
    this.doShowHostDialog_(anchorElement, site);
  },

  /** @private */
  onActionMenuRemoveClick_: function() {
    this.delegate.removeRuntimeHostPermission(
        this.itemId, assert(this.actionMenuModel_, 'Action Menu Model'));
    this.closeActionMenu_();
  },

  /** @private */
  closeActionMenu_: function() {
    const menu = this.$.hostActionMenu;
    assert(menu.open);
    menu.close();
  },

  /** @private */
  onActionMenuClose_: function() {
    this.actionMenuModel_ = null;
    this.actionMenuAnchorElement_ = null;
  },
});
