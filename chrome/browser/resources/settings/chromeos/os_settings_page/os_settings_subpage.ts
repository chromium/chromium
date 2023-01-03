// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-subpage' shows a subpage beneath a subheader. The header
 * contains the subpage title, a search field and a back icon.
 */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../../settings_shared.css.js';
import '../../site_favicon.js';

import {CrSearchFieldElement} from '//resources/cr_elements/cr_search_field/cr_search_field.js';
import {FindShortcutMixin, FindShortcutMixinInterface} from '//resources/cr_elements/find_shortcut_mixin.js';
import {I18nMixin, I18nMixinInterface} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert_ts.js';
import {focusWithoutInk} from '//resources/js/focus_without_ink.js';
import {listenOnce} from '//resources/js/util_ts.js';
import {IronResizableBehavior} from '//resources/polymer/v3_0/iron-resizable-behavior/iron-resizable-behavior.js';
import {afterNextRender, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {Constructor} from '../common/types.js';
import {RouteObserverMixin, RouteObserverMixinInterface} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';
import {getSettingIdParameter} from '../setting_id_param_util.js';

import {getTemplate} from './os_settings_subpage.html.js';

export interface OsSettingsSubpageElement {
  $: {
    closeButton: HTMLElement,
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
    return 'os-settings-subpage';
  }

  static get properties() {
    return {
      pageTitle: String,

      /** Setting this will display the icon at the given URL. */
      titleIcon: String,

      /** Setting this will display the favicon of the website. */
      faviconSiteUrl: String,

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
       * Whether we should hide the "close" button to get to the previous page.
       */
      hideCloseButton: {
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
    };
  }

  pageTitle: string;
  titleIcon: string;
  faviconSiteUrl: string;
  learnMoreUrl: string;
  searchLabel: string;
  searchTerm: string;
  showSpinner: boolean;
  spinnerTitle: string;
  hideCloseButton: boolean;
  associatedControl: HTMLElement|null;
  preserveSearchTerm: boolean;
  private active_: boolean;
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
  private restoreSearchInput_() {
    const searchField = this.shadowRoot!.querySelector('cr-search-field')!;
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
  focusBackButton() {
    if (this.hideCloseButton) {
      return;
    }
    afterNextRender(this, () => focusWithoutInk(this.$.closeButton));
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
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
  private onClearSubpageSearch_(e: Event) {
    e.stopPropagation();
    this.shadowRoot!.querySelector('cr-search-field')!.setValue('');
  }

  private onBackClick_() {
    Router.getInstance().navigateToPreviousRoute();
  }

  private onHelpClick_() {
    window.open(this.learnMoreUrl);
  }

  private onSearchChanged_(e: CustomEvent<string>) {
    if (this.searchTerm === e.detail) {
      return;
    }

    this.searchTerm = e.detail;
    if (this.preserveSearchTerm && this.active_) {
      this.preserveSearchInput_();
    }
  }

  private getBackButtonAriaLabel_() {
    return this.i18n('subpageBackButtonAriaLabel', this.pageTitle);
  }

  private getBackButtonAriaRoleDescription_() {
    return this.i18n('subpageBackButtonAriaRoleDescription', this.pageTitle);
  }

  private getLearnMoreAriaLabel_() {
    return this.i18n('subpageLearnMoreAriaLabel', this.pageTitle);
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean) {
    if (modalContextOpen) {
      return false;
    }
    this.shadowRoot!.querySelector('cr-search-field')!.getSearchInput().focus();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus() {
    const field = this.shadowRoot!.querySelector('cr-search-field')!;
    return field.getSearchInput() === field.shadowRoot!.activeElement;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-subpage': OsSettingsSubpageElement;
  }
}

customElements.define(OsSettingsSubpageElement.is, OsSettingsSubpageElement);
