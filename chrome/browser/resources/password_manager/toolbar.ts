// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.css.js';
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Page, Route, RouteObserverMixin, Router, UrlParam} from './router.js';
import {getTemplate} from './toolbar.html.js';

export interface PasswordManagerToolbarElement {
  $: {
    mainToolbar: CrToolbarElement,
  };
}

export class PasswordManagerToolbarElement extends RouteObserverMixin
(PolymerElement) {
  static get is() {
    return 'password-manager-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      narrow: Boolean,
    };
  }

  narrow: boolean;

  override currentRouteChanged(newRoute: Route, _oldRoute: Route): void {
    this.updateSearchTerm(newRoute.queryParameters);
  }

  get searchField(): CrToolbarSearchFieldElement {
    return this.$.mainToolbar.getSearchField();
  }

  private onSearchChanged_(event: CustomEvent<string>) {
    const newParams = Router.getInstance().currentRoute.queryParameters;
    if (event.detail) {
      newParams.set(UrlParam.SEARCH_TERM, event.detail);
      // Switch to passwords page, since search is supported only on passwords.
      Router.getInstance().navigateTo(Page.PASSWORDS);
    } else {
      newParams.delete(UrlParam.SEARCH_TERM);
    }
    Router.getInstance().updateRouterParams(newParams);
  }

  private updateSearchTerm(query: URLSearchParams) {
    const searchTerm = query.get(UrlParam.SEARCH_TERM) || '';
    if (searchTerm !== this.searchField.getValue()) {
      this.searchField.setValue(searchTerm);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-manager-toolbar': PasswordManagerToolbarElement;
  }
}

customElements.define(
    PasswordManagerToolbarElement.is, PasswordManagerToolbarElement);
