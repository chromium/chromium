// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import type {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Route} from './router.js';
import {Page, RouteObserverMixinLit, Router, UrlParam} from './router.js';
import {getCss} from './toolbar.css.js';
import {getHtml} from './toolbar.html.js';

export interface PasswordManagerToolbarElement {
  $: {
    mainToolbar: CrToolbarElement,
  };
}

const PasswordManagerToolbarElementBase =
    I18nMixinLit(RouteObserverMixinLit(CrLitElement));

export class PasswordManagerToolbarElement extends
    PasswordManagerToolbarElementBase {
  static get is() {
    return 'password-manager-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      narrow: {type: Boolean},
      pageName: {type: String},
    };
  }

  accessor narrow: boolean = false;
  accessor pageName: string = '';

  override currentRouteChanged(newRoute: Route, _oldRoute: Route): void {
    this.updateSearchTerm(newRoute.queryParameters);
  }

  get searchField(): CrToolbarSearchFieldElement {
    return this.$.mainToolbar.getSearchField();
  }

  protected onSearchChanged_(event: CustomEvent<string>) {
    const newParams = new URLSearchParams();
    if (event.detail) {
      newParams.set(UrlParam.SEARCH_TERM, event.detail);
      // Switch to passwords page, since search is supported only on passwords.
      if (Router.getInstance().currentRoute.page !== Page.PASSWORDS) {
        Router.getInstance().navigateTo(Page.PASSWORDS, null, newParams);
        return;
      }
    }
    Router.getInstance().updateRouterParams(newParams);
  }

  private updateSearchTerm(query: URLSearchParams) {
    const searchTerm = query.get(UrlParam.SEARCH_TERM) || '';
    if (searchTerm !== this.searchField.getValue()) {
      this.searchField.setValue(searchTerm);
    }
  }

  protected onHelpClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        this.i18n('passwordManagerLearnMoreURL'));
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.fire('search-enter-click');
      e.preventDefault();
    }
  }
}

export type ToolbarElement = PasswordManagerToolbarElement;

declare global {
  interface HTMLElementTagNameMap {
    'password-manager-toolbar': PasswordManagerToolbarElement;
  }
}

customElements.define(
    PasswordManagerToolbarElement.is, PasswordManagerToolbarElement);
