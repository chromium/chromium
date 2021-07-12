// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-subpage' shows a subpage beneath a subheader. The header contains
 * the subpage title, a search field and a back icon.
 */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared_css.js';

import {CrSearchFieldElement} from '//resources/cr_elements/cr_search_field/cr_search_field.js';
import {FindShortcutBehavior, FindShortcutBehaviorInterface} from '//resources/cr_elements/find_shortcut_behavior.js';
import {assert} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {listenOnce} from '//resources/js/util.m.js';
import {IronResizableBehavior} from '//resources/polymer/v3_0/iron-resizable-behavior/iron-resizable-behavior.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';

import {loadTimeData} from '../i18n_setup.js';
import {RouteObserverMixin, Router} from '../router.js';
import {getSettingIdParameter} from '../setting_id_param_util.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {FindShortcutBehaviorInterface}
 */
const SettingsSubpageElementBase = mixinBehaviors(
    [FindShortcutBehavior, I18nBehavior, IronResizableBehavior],
    RouteObserverMixin(PolymerElement));

/** @polymer */
class SettingsSubpageElement extends SettingsSubpageElementBase {
  static get is() {
    return 'settings-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      pageTitle: String,

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
       * @type {?HTMLElement}
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

      /** @private */
      active_: {
        type: Boolean,
        value: false,
        observer: 'onActiveChanged_',
      },
    };
  }

  constructor() {
    super();

    /** @private {boolean} */
    this.lastActiveValue_ = false;

    // Override FindShortcutBehavior property.
    this.findShortcutListenOnAttach = false;

    /** @private {?EventTracker} */
    this.eventTracker_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    if (this.searchLabel) {
      // |searchLabel| should not change dynamically.
      this.eventTracker_ = new EventTracker();
      this.eventTracker_.add(
          this, 'clear-subpage-search', this.onClearSubpageSearch_);
    }
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    if (this.searchLabel) {
      // |searchLabel| should not change dynamically.
      this.eventTracker_.removeAll();
    }
  }

  /**
   * @return {!Promise<!CrSearchFieldElement>}
   * @private
   */
  getSearchField_() {
    let searchField = this.shadowRoot.querySelector('cr-search-field');
    if (searchField) {
      return Promise.resolve(searchField);
    }

    return new Promise(resolve => {
      listenOnce(this, 'dom-change', () => {
        searchField = this.shadowRoot.querySelector('cr-search-field');
        resolve(assert(searchField));
      });
    });
  }

  /**
   * Restore search field value from URL search param
   * @private
   */
  restoreSearchInput_() {
    const searchField = this.shadowRoot.querySelector('cr-search-field');
    if (assert(searchField)) {
      const urlSearchQuery =
          Router.getInstance().getQueryParameters().get('searchSubpage') || '';
      this.searchTerm = urlSearchQuery;
      searchField.setValue(urlSearchQuery);
    }
  }

  /**
   * Preserve search field value to URL search param
   * @private
   */
  preserveSearchInput_() {
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

  /** @protected */
  currentRouteChanged(newRoute, oldRoute) {
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

  /** @private */
  onActiveChanged_() {
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

  /**
   * Clear the value of the search field.
   * @param {!Event} e
   */
  onClearSubpageSearch_(e) {
    e.stopPropagation();
    this.shadowRoot.querySelector('cr-search-field').setValue('');
  }

  /** @private */
  onBackClick_() {
    Router.getInstance().navigateToPreviousRoute();
  }

  /** @private */
  onHelpClick_() {
    window.open(this.learnMoreUrl);
  }

  /** @private */
  onSearchChanged_(e) {
    if (this.searchTerm === e.detail) {
      return;
    }

    this.searchTerm = e.detail;
    if (this.preserveSearchTerm && this.active_) {
      this.preserveSearchInput_();
    }
  }

  /** @private */
  getBackButtonAriaLabel_() {
    return this.i18n('subpageBackButtonAriaLabel', this.pageTitle);
  }

  /** @private */
  getBackButtonAriaRoleDescription_() {
    return this.i18n('subpageBackButtonAriaRoleDescription', this.pageTitle);
  }

  // Override FindShortcutBehavior methods.
  handleFindShortcut(modalContextOpen) {
    if (modalContextOpen) {
      return false;
    }
    this.shadowRoot.querySelector('cr-search-field').getSearchInput().focus();
    return true;
  }

  // Override FindShortcutBehavior methods.
  searchInputHasFocus() {
    const field = this.shadowRoot.querySelector('cr-search-field');
    return field.getSearchInput() === field.shadowRoot.activeElement;
  }
}

customElements.define(SettingsSubpageElement.is, SettingsSubpageElement);
