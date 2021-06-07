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

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';

/** Event interface for dom-repeat. */
interface RepeaterEvent extends CustomEvent {
  model: {
    item: string,
  };
}

interface ExtensionsRuntimeHostPermissionsElement {
  $: {
    hostActionMenu: CrActionMenuElement,
    'host-access': CrRadioGroupElement,
  };
}

class ExtensionsRuntimeHostPermissionsElement extends PolymerElement {
  static get is() {
    return 'extensions-runtime-host-permissions';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The underlying permissions data.
       */
      permissions: Object,

      itemId: String,

      delegate: Object,

      /**
       * Whether the dialog to add a new host permission is shown.
       */
      showHostDialog_: Boolean,

      /**
       * The current site of the entry that the host dialog is editing, if the
       * dialog is open for editing.
       */
      hostDialogModel_: {
        type: String,
        value: null,
      },

      /**
       * The element to return focus to once the host dialog closes.
       */
      hostDialogAnchorElement_: {
        type: Object,
        value: null,
      },

      /**
       * If the action menu is open, the site of the entry it is open for.
       * Otherwise null.
       */
      actionMenuModel_: {
        type: String,
        value: null,
      },

      /**
       * The element that triggered the action menu, so that the page will
       * return focus once the action menu (or dialog) closes.
       */
      actionMenuAnchorElement_: {
        type: Object,
        value: null,
      },

      /**
       * The old host access setting; used when we don't immediately commit the
       * change to host access so that we can reset it if the user cancels.
       */
      oldHostAccess_: {
        type: String,
        value: null,
      },

      /**
       * Indicator to track if an onHostAccessChange_ event is coming from the
       * setting being automatically reverted to the previous value, after a
       * change to a new value was canceled.
       */
      revertingHostAccess_: {
        type: Boolean,
        value: false,
      },

      /**
       * Proxying the enum to be used easily by the html template.
       */
      HostAccess_: {
        type: Object,
        value: chrome.developerPrivate.HostAccess,
      },
    };
  }

  permissions: chrome.developerPrivate.RuntimeHostPermissions;
  itemId: string;
  delegate: ItemDelegate;
  private showHostDialog_: boolean;
  private hostDialogModel_: string|null;
  private hostDialogAnchorElement_: HTMLElement|null;
  private actionMenuModel_: string|null;
  private actionMenuAnchorElement_: HTMLElement|null;
  private oldHostAccess_: string|null;
  private revertingHostAccess_: boolean;

  private onHostAccessChange_() {
    const group = this.$['host-access'];
    const access = group.selected as chrome.developerPrivate.HostAccess;

    // Log a user action when the host access selection is changed by the user,
    // but not when reverting from a canceled change to another setting.
    if (!this.revertingHostAccess_) {
      switch (access) {
        case chrome.developerPrivate.HostAccess.ON_CLICK:
          chrome.metricsPrivate.recordUserAction(
              'Extensions.Settings.Hosts.OnClickSelected');
          break;
        case chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES:
          chrome.metricsPrivate.recordUserAction(
              'Extensions.Settings.Hosts.OnSpecificSitesSelected');
          break;
        case chrome.developerPrivate.HostAccess.ON_ALL_SITES:
          chrome.metricsPrivate.recordUserAction(
              'Extensions.Settings.Hosts.OnAllSitesSelected');
          break;
      }
    }

    if (access === chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES &&
        this.permissions.hostAccess !==
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
  }

  private showSpecificSites_(): boolean {
    return this.permissions.hostAccess ===
        chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES;
  }

  /**
   * @return The granted host permissions as a sorted set of strings.
   */
  private getRuntimeHosts_(): string[] {
    if (!this.permissions.hosts) {
      return [];
    }

    // Only show granted hosts in the list.
    // TODO(devlin): For extensions that request a finite set of hosts,
    // display them in a toggle list. https://crbug.com/891803.
    return this.permissions.hosts.filter(control => control.granted)
        .map(control => control.host)
        .sort();
  }

  private onAddHostClick_(e: Event) {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.AddHostActivated');
    this.doShowHostDialog_(e.target as HTMLElement, null);
  }

  /**
   * @param anchorElement The element to return focus to once the dialog closes.
   * @param currentSite The site entry currently being edited, or null if this
   *     is to add a new entry.
   */
  private doShowHostDialog_(
      anchorElement: HTMLElement, currentSite: string|null) {
    this.hostDialogAnchorElement_ = anchorElement;
    this.hostDialogModel_ = currentSite;
    this.showHostDialog_ = true;
  }

  private onHostDialogClose_() {
    this.hostDialogModel_ = null;
    this.showHostDialog_ = false;
    focusWithoutInk(assert(this.hostDialogAnchorElement_!, 'Host Anchor'));
    this.hostDialogAnchorElement_ = null;
    this.oldHostAccess_ = null;
  }

  private onHostDialogCancel_() {
    // The user canceled the dialog. Set host-access back to the old value,
    // if the dialog was shown when just transitioning to a new state.
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.AddHostDialogCanceled');
    if (this.oldHostAccess_) {
      assert(this.permissions.hostAccess === this.oldHostAccess_);
      this.revertingHostAccess_ = true;
      this.$['host-access'].selected = this.oldHostAccess_;
      this.revertingHostAccess_ = false;
      this.oldHostAccess_ = null;
    }
  }

  private dialogShouldUpdateHostAccess_(): boolean {
    return !!this.oldHostAccess_;
  }

  private onEditHostClick_(e: RepeaterEvent) {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.ActionMenuOpened');
    this.actionMenuModel_ = e.model.item;
    this.actionMenuAnchorElement_ = e.target as HTMLElement;
    this.$.hostActionMenu.showAt(e.target as HTMLElement);
  }

  private onActionMenuEditClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.ActionMenuEditActivated');
    // Cache the site before closing the action menu, since it's cleared.
    const site = this.actionMenuModel_;

    // Cache and reset actionMenuAnchorElement_ so focus is not returned
    // to the action menu's trigger (since the dialog will be shown next).
    // Instead, curry the element to the dialog, so once it closes, focus
    // will be returned.
    const anchorElement = assert(this.actionMenuAnchorElement_!, 'Menu Anchor');
    this.actionMenuAnchorElement_ = null;
    this.closeActionMenu_();
    this.doShowHostDialog_(anchorElement, site);
  }

  private onActionMenuRemoveClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.ActionMenuRemoveActivated');
    this.delegate.removeRuntimeHostPermission(
        this.itemId, assert(this.actionMenuModel_!, 'Action Menu Model'));
    this.closeActionMenu_();
  }

  private closeActionMenu_() {
    const menu = this.$.hostActionMenu;
    assert(menu.open);
    menu.close();
  }

  private onLearnMoreClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.LearnMoreActivated');
  }
}

customElements.define(
    ExtensionsRuntimeHostPermissionsElement.is,
    ExtensionsRuntimeHostPermissionsElement);
