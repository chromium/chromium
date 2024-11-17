// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './runtime_hosts_dialog.js';
import '/strings.m.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ItemDelegate} from './item.js';
import {DummyItemDelegate} from './item.js';
import {getCss} from './runtime_host_permissions.css.js';
import {getHtml} from './runtime_host_permissions.html.js';
import {getFaviconUrl} from './url_util.js';

function getEventTargetIndex(e: Event): number {
  return Number((e.target as HTMLElement).dataset['index']);
}

export interface ExtensionsRuntimeHostPermissionsElement {
  $: {
    hostActionMenu: CrActionMenuElement,
  };
}

export class ExtensionsRuntimeHostPermissionsElement extends CrLitElement {
  static get is() {
    return 'extensions-runtime-host-permissions';
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
       * The underlying permissions data.
       */
      permissions: {type: Object},
      itemId: {type: String},
      delegate: {type: Object},
      enableEnhancedSiteControls: {type: Boolean},

      /**
       * Whether the dialog to add a new host permission is shown.
       */
      showHostDialog_: {type: Boolean},

      /**
       * Whether the dialog warning the user that the list of sites added will
       * be removed is shown.
       */
      showRemoveSiteDialog_: {type: Boolean},

      /**
       * The current site of the entry that the host dialog is editing, if the
       * dialog is open for editing.
       */
      hostDialogModel_: {type: String},

      /**
       * The element to return focus to once the host dialog closes.
       */
      hostDialogAnchorElement_: {type: Object},

      /**
       * If the action menu is open, the site of the entry it is open for.
       * Otherwise null.
       */
      actionMenuModel_: {type: String},

      /**
       * The element that triggered the action menu, so that the page will
       * return focus once the action menu (or dialog) closes.
       */
      actionMenuAnchorElement_: {type: Object},

      /**
       * The old host access setting; used when we don't immediately commit the
       * change to host access so that we can reset it if the user cancels.
       */
      oldHostAccess_: {type: String},

      /**
       * Indicator to track if an onHostAccessChange_ event is coming from the
       * setting being automatically reverted to the previous value, after a
       * change to a new value was canceled.
       */
      revertingHostAccess_: {type: Boolean},
    };
  }

  permissions: chrome.developerPrivate.RuntimeHostPermissions = {
    hasAllHosts: true,
    hostAccess: chrome.developerPrivate.HostAccess.ON_CLICK,
    hosts: [],
  };
  itemId: string = '';
  delegate: ItemDelegate = new DummyItemDelegate();
  enableEnhancedSiteControls: boolean = false;
  protected showHostDialog_: boolean = false;
  protected showRemoveSiteDialog_: boolean = false;
  protected hostDialogModel_: string|null = null;
  private hostDialogAnchorElement_: HTMLElement|null = null;
  private actionMenuModel_: string|null = null;
  private actionMenuAnchorElement_: HTMLElement|null = null;
  private oldHostAccess_: string|null = null;
  private revertingHostAccess_: boolean = false;

  getSelectMenu(): HTMLSelectElement {
    const selectMenuId =
        this.enableEnhancedSiteControls ? '#newHostAccess' : '#hostAccess';
    return this.shadowRoot!.querySelector<HTMLSelectElement>(selectMenuId)!;
  }

  getRemoveSiteDialog(): CrDialogElement {
    return this.shadowRoot!.querySelector<CrDialogElement>(
        '#removeSitesDialog')!;
  }

  protected onHostAccessChange_() {
    const selectMenu = this.getSelectMenu();
    const access = selectMenu.value as chrome.developerPrivate.HostAccess;

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

    const kOnSpecificSites =
        chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES;
    if (access === kOnSpecificSites &&
        this.permissions.hostAccess !== kOnSpecificSites) {
      // If the user is transitioning to the "on specific sites" option, show
      // the "add host" dialog. This serves two purposes:
      // - The user is prompted to add a host immediately, since otherwise
      //   "on specific sites" is meaningless, and
      // - The way the C++ code differentiates between "on click" and "on
      //   specific sites" is by checking if there are any specific sites.
      //   This ensures there will be at least one, so that the host access
      //   is properly calculated.
      this.oldHostAccess_ = this.permissions.hostAccess;
      this.doShowHostDialog_(selectMenu, null);
    } else if (
        this.enableEnhancedSiteControls && access !== kOnSpecificSites &&
        this.permissions.hostAccess === kOnSpecificSites) {
      // If the user is transitioning from the "on specific sites" option to
      // another one, show a dialog asking the user to confirm the transition
      // because in C++, only the "on specific sites" option will store sites
      // the user has added and transitioning away from it will clear these
      // sites.
      this.showRemoveSiteDialog_ = true;
    } else {
      this.delegate.setItemHostAccess(this.itemId, access);
    }
  }

  protected showSpecificSites_(): boolean {
    return this.permissions.hostAccess ===
        chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES;
  }

  /**
   * @return The granted host permissions as a sorted set of strings.
   */
  protected getRuntimeHosts_(): string[] {
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

  protected onAddHostClick_(e: Event) {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.AddHostActivated');
    this.doShowHostDialog_(e.target as HTMLElement, null);
  }

  protected isHostAccessSelected_(access: chrome.developerPrivate.HostAccess) {
    return access === this.permissions.hostAccess;
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

  protected onHostDialogClose_() {
    this.hostDialogModel_ = null;
    this.showHostDialog_ = false;
    assert(this.hostDialogAnchorElement_);
    focusWithoutInk(this.hostDialogAnchorElement_);
    this.hostDialogAnchorElement_ = null;
    this.oldHostAccess_ = null;
  }

  protected onHostDialogCancel_() {
    // The user canceled the dialog. Set hostAccess back to the old value,
    // if the dialog was shown when just transitioning to a new state.
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.AddHostDialogCanceled');
    if (this.oldHostAccess_) {
      assert(this.permissions.hostAccess === this.oldHostAccess_);
      this.revertingHostAccess_ = true;
      this.getSelectMenu().value = this.oldHostAccess_;
      this.revertingHostAccess_ = false;
      this.oldHostAccess_ = null;
    }
  }

  protected dialogShouldUpdateHostAccess_(): boolean {
    return !!this.oldHostAccess_;
  }

  protected onOpenEditHostClick_(e: Event) {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.ActionMenuOpened');
    const index = getEventTargetIndex(e);
    this.actionMenuModel_ = this.getRuntimeHosts_()[index] || null;
    this.actionMenuAnchorElement_ = e.target as HTMLElement;
    this.$.hostActionMenu.showAt(e.target as HTMLElement);
  }

  protected onActionMenuEditClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.ActionMenuEditActivated');
    // Cache the site before closing the action menu, since it's cleared.
    const site = this.actionMenuModel_;

    // Cache and reset actionMenuAnchorElement_ so focus is not returned
    // to the action menu's trigger (since the dialog will be shown next).
    // Instead, curry the element to the dialog, so once it closes, focus
    // will be returned.
    assert(this.actionMenuAnchorElement_, 'Menu Anchor');
    const anchorElement = this.actionMenuAnchorElement_;
    this.actionMenuAnchorElement_ = null;
    this.closeActionMenu_();
    this.doShowHostDialog_(anchorElement, site);
  }

  protected onActionMenuRemoveClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.ActionMenuRemoveActivated');
    assert(this.actionMenuModel_, 'Action Menu Model');
    this.delegate.removeRuntimeHostPermission(
        this.itemId, this.actionMenuModel_);
    this.closeActionMenu_();
  }

  private closeActionMenu_() {
    const menu = this.$.hostActionMenu;
    assert(menu.open);
    menu.close();
  }

  protected onLearnMoreClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Settings.Hosts.LearnMoreActivated');
  }

  protected onEditHostClick_(e: Event) {
    const index = getEventTargetIndex(e);
    const item = this.getRuntimeHosts_()[index]!;
    this.doShowHostDialog_(e.target as HTMLElement, item);
  }

  protected onDeleteHostClick_(e: Event) {
    const index = getEventTargetIndex(e);
    const item = this.getRuntimeHosts_()[index]!;
    this.delegate.removeRuntimeHostPermission(this.itemId, item);
  }

  protected getFaviconUrl_(url: string): string {
    return getFaviconUrl(url);
  }

  protected onRemoveSitesWarningConfirm_() {
    this.delegate.setItemHostAccess(
        this.itemId,
        this.getSelectMenu().value as chrome.developerPrivate.HostAccess);
    this.getRemoveSiteDialog().close();
    this.showRemoveSiteDialog_ = false;
  }

  protected onRemoveSitesWarningCancel_() {
    assert(
        this.permissions.hostAccess ===
        chrome.developerPrivate.HostAccess.ON_SPECIFIC_SITES);
    this.revertingHostAccess_ = true;
    this.getSelectMenu().value = this.permissions.hostAccess;
    this.revertingHostAccess_ = false;
    this.getRemoveSiteDialog().close();
    this.showRemoveSiteDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-runtime-host-permissions':
        ExtensionsRuntimeHostPermissionsElement;
  }
}

customElements.define(
    ExtensionsRuntimeHostPermissionsElement.is,
    ExtensionsRuntimeHostPermissionsElement);
