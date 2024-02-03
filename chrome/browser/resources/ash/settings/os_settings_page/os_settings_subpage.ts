// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-subpage' shows a subpage beneath a subheader. The header
 * contains the subpage title, a search field and a back icon.
 */

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared.css.js';
import './settings_card.js';

import {CrSearchFieldElement} from 'chrome://resources/ash/common/cr_elements/cr_search_field/cr_search_field.js';
import {FindShortcutMixin, FindShortcutMixinInterface} from 'chrome://resources/ash/common/cr_elements/find_shortcut_mixin.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {IronResizableBehavior} from 'chrome://resources/polymer/v3_0/iron-resizable-behavior/iron-resizable-behavior.js';
import {afterNextRender, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../common/route_observer_mixin.js';
import {getSettingIdParameter} from '../common/setting_id_param_util.js';
import {Constructor} from '../common/types.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './os_settings_subpage.html.js';

export interface OsSettingsSubpageElement {
  $: {
    backButton: HTMLButtonElement,
  };
}

const OsSettingsSubpageElementBase =
    mixinBehaviors(
        [IronResizableBehavior],
        RouteObserverMixin(FindShortcutMixin(I18nMixin(PolymerElement)))) as
    Constructor<PolymerElement&FindShortcutMixinInterface&I18nMixinInterface&
                RouteObserverMixinInterface>;

export class OsSettingsSubpageElement extends OsSettingsSubpageElementBase {
  static get is() {
    return 'os-settings-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pageTitle: String,

      /** Setting this will display the icon at the given URL. */
      titleIcon: String,

      learnMoreUrl: String,

      /** Setting a |searchLabel| will enable search. */
      searchLabel: String,

      searchTerm: {
        type: String,
        notify: true,
        value: '',
      },

      /** If true shows an active spinner at the end of the subpage header. */
      showSpinner: {
        type: Boolean,
        value: false,
      },

      /**
       * Title (i.e., tooltip) to be displayed on the spinner. If |showSpinner|
       * is false, this field has no effect.
       */
      spinnerTitle: {
        type: String,
        value: '',
      },

      /**
       * Whether the back button, which goes to the previous page, should be
       * hidden.
       */
      hideBackButton: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates which element triggers this subpage. Used by the searching
       * algorithm to show search bubbles. It is |null| for subpages that are
       * skipped during searching.
       */
      associatedControl: {
        type: Object,
        value: null,
      },

      /**
       * Whether the subpage search term should be preserved across navigations.
       */
      preserveSearchTerm: {
        type: Boolean,
        value: false,
      },

      active_: {
        type: Boolean,
        value: false,
        observer: 'onActiveChanged_',
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value() {
          return isRevampWayfindingEnabled();
        },
        readOnly: true,
      },
    };
  }

  pageTitle: string;
  titleIcon: string;
  learnMoreUrl: string;
  searchLabel: string;
  searchTerm: string;
  showSpinner: boolean;
  spinnerTitle: string;
  hideBackButton: boolean;
  associatedControl: HTMLElement|null;
  preserveSearchTerm: boolean;
  private active_: boolean;
  private lastActiveValue_: boolean = false;
  private eventTracker_: EventTracker|null = null;
  private readonly isRevampWayfindingEnabled_: boolean;

  constructor() {
    super();

    // Override FindShortcutMixin property.
    this.findShortcutListenOnAttach = false;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    if (this.searchLabel) {
      // |searchLabel| should not change dynamically.
      this.eventTracker_ = new EventTracker();
      this.eventTracker_.add(
          this, 'clear-subpage-search', this.onClearSubpageSearch_);
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    if (this.eventTracker_) {
      // |searchLabel| should not change dynamically.
      this.eventTracker_.removeAll();
    }
  }

  private getSearchField_(): Promise<CrSearchFieldElement> {
    let searchField = this.shadowRoot!.querySelector('cr-search-field');
    if (searchField) {
      return Promise.resolve(searchField);
    }

    return new Promise(resolve => {
      listenOnce(this, 'dom-change', () => {
        searchField = this.shadowRoot!.querySelector('cr-search-field');
        assert(!!searchField);
        resolve(searchField);
      });
    });
  }

  /** Restore search field value from URL search param */
  private restoreSearchInput_(): void {
    const searchField = this.shadowRoot!.querySelector('cr-search-field')!;
    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('searchSubpage') || '';
    this.searchTerm = urlSearchQuery;
    searchField.setValue(urlSearchQuery);
  }

  /** Preserve search field value to URL search param */
  private preserveSearchInput_(): void {
    const query = this.searchTerm;
    const searchParams = query.length > 0 ?
        new URLSearchParams('searchSubpage=' + encodeURIComponent(query)) :
        undefined;
    const currentRoute = Router.getInstance().currentRoute;
    Router.getInstance().navigateTo(currentRoute, searchParams);
  }

  /** Focuses the back button when page is loaded. */
  focusBackButton(): void {
    if (this.hideBackButton) {
      return;
    }
    afterNextRender(this, () => focusWithoutInk(this.$.backButton));
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    this.active_ = this.getAttribute('route-path') === newRoute.path;
    if (this.active_ && this.searchLabel && this.preserveSearchTerm) {
      this.getSearchField_().then(() => this.restoreSearchInput_());
    }
    if (!oldRoute && !getSettingIdParameter()) {
      // If a settings subpage is opened directly (i.e the |oldRoute| is null,
      // e.g via an OS settings search result that surfaces from the Chrome OS
      // launcher, or linking from other places of Chrome UI), the back button
      // should be focused since it's the first actionable element in the the
      // subpage. An exception is when a setting is deep linked, focus that
      // setting instead of back button.
      this.focusBackButton();
    }
  }

  private onActiveChanged_(): void {
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

    const searchField = this.shadowRoot!.querySelector('cr-search-field');
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
  private onClearSubpageSearch_(e: Event): void {
    e.stopPropagation();
    this.shadowRoot!.querySelector('cr-search-field')!.setValue('');
  }

  private onBackClick_(): void {
    Router.getInstance().navigateToPreviousRoute();
  }

  private onHelpClick_(): void {
    window.open(this.learnMoreUrl);
  }

  private onSearchChanged_(e: CustomEvent<string>): void {
    if (this.searchTerm === e.detail) {
      return;
    }

    this.searchTerm = e.detail;
    if (this.preserveSearchTerm && this.active_) {
      this.preserveSearchInput_();
    }
  }

  private getBackButtonAriaLabel_(): string {
    return this.i18n('subpageBackButtonAriaLabel', this.pageTitle);
  }

  private getBackButtonAriaRoleDescription_(): string {
    return this.i18n('subpageBackButtonAriaRoleDescription', this.pageTitle);
  }

  private getLearnMoreAriaLabel_(): string {
    return this.i18n('subpageLearnMoreAriaLabel', this.pageTitle);
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen) {
      return false;
    }
    this.shadowRoot!.querySelector('cr-search-field')!.getSearchInput().focus();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus(): boolean {
    const field = this.shadowRoot!.querySelector('cr-search-field')!;
    return field.getSearchInput() === field.shadowRoot!.activeElement;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsSubpageElement.is]: OsSettingsSubpageElement;
  }
}

customElements.define(OsSettingsSubpageElement.is, OsSettingsSubpageElement);
