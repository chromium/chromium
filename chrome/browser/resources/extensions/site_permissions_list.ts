// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';
import './shared_style.js';
import './shared_vars.js';
import './site_permissions_add_site_dialog.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {DomRepeatEvent, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SitePermissionsDelegate} from './site_permissions.js';
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
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      delegate: Object,
      header: String,
      siteSet: String,
      sites: Array,

      showAddSiteDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  delegate: SitePermissionsDelegate;
  header: string;
  siteSet: chrome.developerPrivate.UserSiteSet;
  sites: Array<string>;
  private showAddSiteDialog_: boolean;
  private siteToEdit_: string|null;

  private hasSites_(): boolean {
    return !!this.sites.length;
  }

  private getFaviconUrl_(url: string): string {
    return getFaviconUrl(url);
  }

  private onAddSiteClick_() {
    this.showAddSiteDialog_ = true;
  }

  private onAddSiteDialogClose_() {
    this.showAddSiteDialog_ = false;
  }

  private onDotsClick_(e: DomRepeatEvent<string>) {
    this.siteToEdit_ = e.model.item;
    this.$.siteActionMenu.showAt(e.target as HTMLElement);
  }

  private onActionMenuRemoveClick_() {
    this.delegate
        .removeUserSpecifiedSite(
            this.siteSet, assert(this.siteToEdit_!, 'Site To Edit'))
        .then(() => {
          this.closeActionMenu_();
        });
  }

  private closeActionMenu_() {
    const menu = this.$.siteActionMenu;
    assert(menu.open);
    menu.close();
    this.siteToEdit_ = null;
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
