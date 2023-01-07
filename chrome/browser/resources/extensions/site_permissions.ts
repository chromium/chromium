// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './strings.m.js';
import './shared_style.css.js';
import './shared_vars.css.js';
import './site_permissions_list.js';

import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from './navigation_helper.js';
import {getTemplate} from './site_permissions.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';

export interface ExtensionsSitePermissionsElement {
  $: {
    allSitesLink: CrLinkRowElement,
  };
}

const ExtensionsSitePermissionsElementBase = SiteSettingsMixin(PolymerElement);

export class ExtensionsSitePermissionsElement extends
    ExtensionsSitePermissionsElementBase {
  static get is() {
    return 'extensions-site-permissions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      extensions: Array,

      siteSetEnum_: {
        type: Object,
        value: chrome.developerPrivate.SiteSet,
      },
    };
  }

  extensions: chrome.developerPrivate.ExtensionInfo[];

  private onAllSitesLinkClick_() {
    navigation.navigateTo({page: Page.SITE_PERMISSIONS_ALL_SITES});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-site-permissions': ExtensionsSitePermissionsElement;
  }
}

customElements.define(
    ExtensionsSitePermissionsElement.is, ExtensionsSitePermissionsElement);
