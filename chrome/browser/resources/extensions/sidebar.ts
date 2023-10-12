// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_nav_menu_item_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './icons.html.js';
import './shared_vars.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from './navigation_helper.js';
import {getTemplate} from './sidebar.html.js';

export interface ExtensionsSidebarElement {
  $: {
    sectionMenu: IronSelectorElement,
    sectionsExtensions: HTMLElement,
    sectionsShortcuts: HTMLElement,
    sectionsSitePermissions: HTMLElement,
    moreExtensions: HTMLElement,
  };
}

const ExtensionsSidebarElementBase = I18nMixin(PolymerElement);

export class ExtensionsSidebarElement extends ExtensionsSidebarElementBase {
  static get is() {
    return 'extensions-sidebar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enableEnhancedSiteControls: Boolean,

      /**
       * The data path/page that identifies the entry to be selected in the
       * sidebar. Note that this may not match the page that's actually
       * displayed.
       */
      selectedPath_: String,

      /**
       * The text displayed in the sidebar containing the link to open the
       * Chrome Web Store to get more extensions.
       */
      discoverMoreText_: {
        type: String,
        computed: 'computeDiscoverMoreText_()',
      },
    };
  }

  enableEnhancedSiteControls: boolean;
  private selectedPath_: Page;
  private discoverMoreText_: TrustedHTML;

  /**
   * The ID of the listener on |navigation|. Stored so that the
   * listener can be removed when this element is detached (happens in tests).
   */
  private navigationListener_: number|null = null;

  override ready() {
    super.ready();
    this.setAttribute('role', 'navigation');
    this.computeSelectedPath_(navigation.getCurrentPage().page);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.navigationListener_ = navigation.addListener(newPage => {
      this.computeSelectedPath_(newPage.page);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.navigationListener_);
    assert(navigation.removeListener(this.navigationListener_));
    this.navigationListener_ = null;
  }

  private computeSelectedPath_(page: Page) {
    switch (page) {
      case Page.SITE_PERMISSIONS:
      case Page.SITE_PERMISSIONS_ALL_SITES:
        this.selectedPath_ = Page.SITE_PERMISSIONS;
        break;
      case Page.SHORTCUTS:
        this.selectedPath_ = Page.SHORTCUTS;
        break;
      default:
        this.selectedPath_ = Page.LIST;
    }
  }

  private onLinkClick_(e: Event) {
    e.preventDefault();
    navigation.navigateTo(
        {page: ((e.target as HTMLElement).dataset['path'] as Page)});
    this.dispatchEvent(
        new CustomEvent('close-drawer', {bubbles: true, composed: true}));
  }

  private onMoreExtensionsClick_(e: Event) {
    if ((e.target as HTMLElement).tagName === 'A') {
      chrome.metricsPrivate.recordUserAction('Options_GetMoreExtensions');
    }
  }

  private computeDiscoverMoreText_(): TrustedHTML {
    return this.i18nAdvanced('sidebarDiscoverMore', {
      tags: ['a'],
      attrs: ['target', 'on-click'],
      substitutions: [loadTimeData.getString('getMoreExtensionsUrl')],
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-sidebar': ExtensionsSidebarElement;
  }
}

customElements.define(ExtensionsSidebarElement.is, ExtensionsSidebarElement);
