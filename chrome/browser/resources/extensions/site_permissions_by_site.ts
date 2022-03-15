// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './shared_style.js';

import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from './navigation_helper.js';
import {getTemplate} from './site_permissions_by_site.html.js';

export interface ExtensionsSitePermissionsBySiteElement {
  $: {
    closeButton: CrIconButtonElement,
  };
}

export class ExtensionsSitePermissionsBySiteElement extends PolymerElement {
  static get is() {
    return 'extensions-site-permissions-by-site';
  }

  static get template() {
    return getTemplate();
  }

  private onCloseButtonClick_() {
    navigation.navigateTo({page: Page.SITE_PERMISSIONS});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-site-permissions-by-site':
        ExtensionsSitePermissionsBySiteElement;
  }
}

customElements.define(
    ExtensionsSitePermissionsBySiteElement.is,
    ExtensionsSitePermissionsBySiteElement);
