// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './strings.m.js';
import './shared_style.css.js';
import './shared_vars.css.js';
import './site_permissions_edit_permissions_dialog.js';
import './site_permissions_edit_url_dialog.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './site_permissions_list.html.js';
import {SiteSettingsDelegate} from './site_settings_mixin.js';
import {getFaviconUrl} from './url_util.js';

export interface ExtensionsSitePermissionsListElement {
  $: {
    addSite: CrButtonElement,
    siteActionMenu: CrActionMenuElement,
  };
}

export class ExtensionsSitePermissionsListElement extends PolymerElement {
  static get is() {
    return 'site-permissions-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,
      extensions: Array,
      header: String,
      siteSet: String,
      sites: Array,

      showEditSiteUrlDialog_: {
        type: Boolean,
        value: false,
      },

      showEditSitePermissionsDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * The site currently being edited if the user has opened the action menu
       * for a given site.
       */
      siteToEdit_: {
        type: String,
        value: null,
      },
    };
  }

  delegate: SiteSettingsDelegate;
  extensions: chrome.developerPrivate.ExtensionInfo[];
  header: string;
  siteSet: chrome.developerPrivate.SiteSet;
  sites: string[];
  private showEditSiteUrlDialog_: boolean;
  private showEditSitePermissionsDialog_: boolean;
  private siteToEdit_: string|null;

  // The element to return focus to once the site input dialog closes. If
  // specified, this is the 3 dots menu for the site just edited, otherwise it's
  // the add site button.
  private siteToEditAnchorElement_: HTMLElement|null = null;

  private hasSites_(): boolean {
    return !!this.sites.length;
  }

  private getFaviconUrl_(url: string): string {
    return getFaviconUrl(url);
  }

  private focusOnAnchor_() {
    // Return focus to the three dots menu once a site has been edited.
    // TODO(crbug.com/1298326): If the edited site is the only site in the
    // list, focus is not on the three dots menu.
    assert(this.siteToEditAnchorElement_, 'Site Anchor');
    focusWithoutInk(this.siteToEditAnchorElement_);
    this.siteToEditAnchorElement_ = null;
  }

  private onAddSiteClick_() {
    assert(!this.showEditSitePermissionsDialog_);
    this.siteToEdit_ = null;
    this.showEditSiteUrlDialog_ = true;
  }

  private onEditSiteUrlDialogClose_() {
    this.showEditSiteUrlDialog_ = false;
    if (this.siteToEdit_ !== null) {
      this.focusOnAnchor_();
    }
    this.siteToEdit_ = null;
  }

  private onEditSitePermissionsDialogClose_() {
    this.showEditSitePermissionsDialog_ = false;
    assert(this.siteToEdit_, 'Site To Edit');
    this.focusOnAnchor_();
    this.siteToEdit_ = null;
  }

  private onDotsClick_(e: DomRepeatEvent<string>) {
    this.siteToEdit_ = e.model.item;
    assert(!this.showEditSitePermissionsDialog_);
    this.$.siteActionMenu.showAt(e.target as HTMLElement);
    this.siteToEditAnchorElement_ = e.target as HTMLElement;
  }

  private onEditSitePermissionsClick_() {
    this.closeActionMenu_();
    assert(this.siteToEdit_ !== null);
    this.showEditSitePermissionsDialog_ = true;
  }

  private onEditSiteUrlClick_() {
    this.closeActionMenu_();
    assert(this.siteToEdit_ !== null);
    this.showEditSiteUrlDialog_ = true;
  }

  private onRemoveSiteClick_() {
    assert(this.siteToEdit_, 'Site To Edit');
    this.delegate.removeUserSpecifiedSites(this.siteSet, [this.siteToEdit_])
        .then(() => {
          this.closeActionMenu_();
          this.siteToEdit_ = null;
        });
  }

  private closeActionMenu_() {
    const menu = this.$.siteActionMenu;
    assert(menu.open);
    menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-permissions-list': ExtensionsSitePermissionsListElement;
  }
}

customElements.define(
    ExtensionsSitePermissionsListElement.is,
    ExtensionsSitePermissionsListElement);
