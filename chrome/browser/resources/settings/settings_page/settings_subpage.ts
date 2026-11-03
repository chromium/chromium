// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-subpage' shows a subpage beneath a subheader. The header contains
 * the subpage title, a search field and a back icon.
 */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_shared_style_lit.css.js';
import '../settings_shared_lit.css.js';
import '../site_favicon.js';

import type {CrSearchFieldElement} from '//resources/cr_elements/cr_search_field/cr_search_field.js';
import {FindShortcutMixinLit} from '//resources/cr_elements/find_shortcut_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';

import {loadTimeData} from '../i18n_setup.js';
import type {Route} from '../router.js';
import {RouteObserverMixinLit, Router} from '../router.js';

import {getCss} from './settings_subpage.css.js';
import {getHtml} from './settings_subpage.html.js';

const SettingsSubpageElementBase =
    RouteObserverMixinLit(FindShortcutMixinLit(I18nMixinLit(CrLitElement)));

export interface SettingsSubpageElement {
  $: {
    closeButton: HTMLElement,
  };
}

export class SettingsSubpageElement extends SettingsSubpageElementBase {
  static get is() {
    return 'settings-subpage';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageTitle: {type: String},

      // Setting this will display the icon at the given URL.
      titleIcon: {type: String},

      // Setting this will display the favicon of the website.
      faviconSiteUrl: {type: String},

      learnMoreUrl: {type: String},

      // Setting a |searchLabel| will enable search.
      searchLabel: {type: String},

      // Setting a |searchIcon| will override the default search icon.
      searchIcon: {type: String},

      searchTerm: {
        type: String,
        notify: true,
      },

      // Whether the subpage search term should be preserved across navigations.
      preserveSearchTerm: {type: Boolean},

      active_: {type: Boolean},
    };
  }

  accessor pageTitle: string = '';
  accessor titleIcon: string = '';
  accessor faviconSiteUrl: string = '';
  accessor learnMoreUrl: string = '';
  accessor searchLabel: string = '';
  accessor searchIcon: string = '';
  accessor searchTerm: string = '';
  accessor preserveSearchTerm: boolean = false;
  private accessor active_: boolean = false;

  private lastActiveValue_: boolean = false;
  private eventTracker_: EventTracker|null = null;

  constructor() {
    super();

    // Override FindShortcutMixin property.
    this.findShortcutListenOnAttach = false;
  }

  override connectedCallback() {
    super.connectedCallback();

    if (this.searchLabel) {
      // |searchLabel| should not change dynamically.
      this.eventTracker_ = new EventTracker();
      this.eventTracker_.add(
          this, 'clear-subpage-search', this.onClearSubpageSearch_);
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.eventTracker_) {
      // |searchLabel| should not change dynamically.
      this.eventTracker_.removeAll();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('active_')) {
      this.onActiveChanged_();
    }
  }

  private async getSearchField_(): Promise<CrSearchFieldElement> {
    let searchField = this.shadowRoot.querySelector('cr-search-field');
    if (searchField) {
      return searchField;
    }

    await this.updateComplete;
    searchField = this.shadowRoot.querySelector('cr-search-field');
    assert(searchField);
    return searchField;
  }

  /** Restore search field value from URL search param */
  private restoreSearchInput_() {
    const searchField = this.shadowRoot.querySelector('cr-search-field')!;
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('searchSubpage') || '';
    this.searchTerm = urlSearchQuery;
    searchField.setValue(urlSearchQuery);
  }

  /** Preserve search field value to URL search param */
  private preserveSearchInput_() {
    const query = this.searchTerm;
    const searchParams = query.length > 0 ?
        new URLSearchParams('searchSubpage=' + encodeURIComponent(query)) :
        undefined;
    const currentRoute = Router.getInstance().getCurrentRoute();
    Router.getInstance().navigateTo(currentRoute, searchParams);
  }

  /** Focuses the back button when page is loaded. */
  async focusBackButton() {
    await this.updateComplete;
    focusWithoutInk(this.$.closeButton);
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    this.active_ = this.getAttribute('route-path') === newRoute.path;
    if (this.active_ && this.searchLabel && this.preserveSearchTerm) {
      this.getSearchField_().then(() => this.restoreSearchInput_());
    }
    if (!oldRoute) {
      // If a settings subpage is opened directly (i.e the |oldRoute| is null,
      // e.g via linking from other places of Chrome UI), the back button should
      // be focused since it's the first actionable element in the the subpage.
      // An exception is when a setting is deep linked, focus that
      // setting instead of back button.
      this.focusBackButton();
    }
  }

  private onActiveChanged_() {
    if (this.lastActiveValue_ === this.active_) {
      return;
    }
    this.lastActiveValue_ = this.active_;

    if (this.active_ && this.pageTitle) {
      document.title =
          loadTimeData.getStringF('settingsAltPageTitle', this.pageTitle);
    }

    if (!this.searchLabel) {
      return;
    }

    const searchField = this.shadowRoot.querySelector('cr-search-field');
    if (searchField) {
      searchField.setValue('');
    }

    if (this.active_) {
      this.becomeActiveFindShortcutListener();
    } else {
      this.removeSelfAsFindShortcutListener();
    }
  }

  /** Clear the value of the search field. */
  private onClearSubpageSearch_(e: Event) {
    e.stopPropagation();
    this.shadowRoot.querySelector('cr-search-field')!.setValue('');
  }

  protected onBackClick_() {
    Router.getInstance().navigateToPreviousRoute();
  }

  protected onHelpClick_() {
    window.open(this.learnMoreUrl);
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    if (this.searchTerm === e.detail) {
      return;
    }

    this.searchTerm = e.detail;
    if (this.preserveSearchTerm && this.active_) {
      this.preserveSearchInput_();
    }
  }

  protected getBackButtonAriaLabel_(): string {
    return this.i18n('subpageBackButtonAriaLabel', this.pageTitle);
  }

  protected getBackButtonAriaRoleDescription_(): string {
    return this.i18n('subpageBackButtonAriaRoleDescription', this.pageTitle);
  }

  protected getLearnMoreAriaLabel_(): string {
    return this.i18n('subpageLearnMoreAriaLabel', this.pageTitle);
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean) {
    if (modalContextOpen) {
      return false;
    }
    this.shadowRoot.querySelector('cr-search-field')!.getSearchInput().focus();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus() {
    const field = this.shadowRoot.querySelector('cr-search-field')!;
    return field.getSearchInput() === field.shadowRoot.activeElement;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-subpage': SettingsSubpageElement;
  }
}

customElements.define(SettingsSubpageElement.is, SettingsSubpageElement);
