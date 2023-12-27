// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_style.css.js';
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import type {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Route} from './router.js';
import {Page, RouteObserverMixin, Router, UrlParam} from './router.js';
import {getTemplate} from './toolbar.html.js';

export interface PasswordManagerToolbarElement {
  $: {
    mainToolbar: CrToolbarElement,
  };
}

const PASSWORD_MANAGER_OVERFLOW_MENU_ELEMENT_ID =
    'PasswordManagerUI::kOverflowMenuElementId';

const PasswordManagerToolbarElementBase =
    HelpBubbleMixin(I18nMixin(RouteObserverMixin(PolymerElement)));

export class PasswordManagerToolbarElement extends
    PasswordManagerToolbarElementBase {
  static get is() {
    return 'password-manager-toolbar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      narrow: Boolean,
      pageName: String,
    };
  }

  narrow: boolean;
  pageName: string;

  override currentRouteChanged(newRoute: Route, _oldRoute: Route): void {
    this.updateSearchTerm(newRoute.queryParameters);
  }

  override ready() {
    super.ready();
    this.$.mainToolbar.addEventListener('dom-change', (e) => {
      const crToolbar = e.target as HTMLElement;
      if (!crToolbar) {
        return;
      }
      const menuButton = crToolbar.shadowRoot?.getElementById('menuButton');
      if (menuButton) {
        this.registerHelpBubble(
            PASSWORD_MANAGER_OVERFLOW_MENU_ELEMENT_ID, menuButton);
      }
    });
  }

  get searchField(): CrToolbarSearchFieldElement {
    return this.$.mainToolbar.getSearchField();
  }

  private onSearchChanged_(event: CustomEvent<string>) {
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

  private onHelpClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        this.i18n('passwordManagerLearnMoreURL'));
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (e.key === 'Enter') {
      this.dispatchEvent(new CustomEvent(
          'search-enter-click', {bubbles: true, composed: true}));
      e.preventDefault();
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
