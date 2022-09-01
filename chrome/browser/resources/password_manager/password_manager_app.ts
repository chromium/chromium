// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './side_bar.js';
import './toolbar.js';

import {IronPagesElement} from 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_manager_app.html.js';
import {Page, Route, RouteObserverMixin} from './router.js';
import {PasswordManagerSideBarElement} from './side_bar.js';
import {PasswordManagerToolbarElement} from './toolbar.js';

export interface PasswordManagerAppElement {
  $: {
    toolbar: PasswordManagerToolbarElement,
    sidebar: PasswordManagerSideBarElement,
    content: IronPagesElement,
  };
}

export class PasswordManagerAppElement extends RouteObserverMixin
(PolymerElement) {
  static get is() {
    return 'password-manager-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedPage_: String,
    };
  }

  private selectedPage_: Page;

  override currentRouteChanged(route: Route): void {
    this.selectedPage_ = route.page;
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'password-manager-app': PasswordManagerAppElement;
  }
}

customElements.define(PasswordManagerAppElement.is, PasswordManagerAppElement);
