// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from './navigation_helper.js';
import {getTemplate} from './sidebar.html.js';

export interface ExtensionsSidebarElement {
  $: {
    sectionMenu: IronSelectorElement,
    sectionsExtensions: HTMLElement,
    sectionsShortcuts: HTMLElement,
  };
}

export class ExtensionsSidebarElement extends PolymerElement {
  static get is() {
    return 'extensions-sidebar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enableEnhancedSiteControls: Boolean,
    };
  }

  enableEnhancedSiteControls: boolean;

  override ready() {
    super.ready();
    this.setAttribute('role', 'navigation');
  }

  override connectedCallback() {
    super.connectedCallback();

    const page = navigation.getCurrentPage().page;
    let selectIndex = 0;
    if (page === Page.SITE_PERMISSIONS ||
        page === Page.SITE_PERMISSIONS_ALL_SITES) {
      selectIndex = 1;
    } else if (page === Page.SHORTCUTS) {
      selectIndex = 2;
    }
    this.$.sectionMenu.select(selectIndex);
  }

  private onLinkTap_(e: Event) {
    e.preventDefault();
    navigation.navigateTo(
        {page: ((e.target as HTMLElement).dataset['path'] as Page)});
    this.dispatchEvent(
        new CustomEvent('close-drawer', {bubbles: true, composed: true}));
  }

  private onMoreExtensionsTap_() {
    chrome.metricsPrivate.recordUserAction('Options_GetMoreExtensions');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-sidebar': ExtensionsSidebarElement;
  }
}

customElements.define(ExtensionsSidebarElement.is, ExtensionsSidebarElement);
