// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-view' is the subpage containing details about the
 * password such as the URL, the username, the password and the note.
 */

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {getTemplate} from './password_view.html.js';

const PasswordViewElementBase = RouteObserverMixin(PolymerElement) as {
  new (): PolymerElement & RouteObserverMixinInterface,
};

export class PasswordViewElement extends PasswordViewElementBase {
  static get is() {
    return 'password-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      credential: {
        type: Object,
        value: null,
        notify: true,
      },
    };
  }

  credential: MultiStorePasswordUiEntry|null;

  override currentRouteChanged(route: Route) {
    if (route !== routes.PASSWORD_VIEW) {
      return;
    }
    const queryParameters = Router.getInstance().getQueryParameters();

    const site = queryParameters.get('site');
    if (!site) {
      return;
    }

    const username = queryParameters.get('username');
    if (!username) {
      return;
    }

    // TODO(https://crbug.com/1298027): Update the credential here based on site
    // and username. The credential below is temporary.
    this.credential = {
      urls: {
        shown: site,
        link: site,
      }
    } as MultiStorePasswordUiEntry;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-view': PasswordViewElement;
  }
}

customElements.define(PasswordViewElement.is, PasswordViewElement);
