// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_menu_selector/cr_menu_selector.js';
import 'chrome://resources/cr_elements/cr_nav_menu_item_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './shared_style.css.js';
import './icons.html.js';

import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Page, Route, RouteObserverMixin, Router} from './router.js';
import {getTemplate} from './side_bar.html.js';

export interface PasswordManagerSideBarElement {
  $: {
    'menu': IronSelectorElement,
  };
}

export class PasswordManagerSideBarElement extends RouteObserverMixin
(PolymerElement) {
  static get is() {
    return 'password-manager-side-bar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // The id of the currently selected page.
      selectedPage_: String,
    };
  }

  private selectedPage_: Page;

  override currentRouteChanged(route: Route, _: Route): void {
    this.selectedPage_ = route.page;
  }

  private onSelectorActivate_(event: CustomEvent<{selected: Page}>) {
    Router.getInstance().navigateTo(event.detail.selected);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-manager-side-bar': PasswordManagerSideBarElement;
  }
}

customElements.define(
    PasswordManagerSideBarElement.is, PasswordManagerSideBarElement);
