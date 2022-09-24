// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared.css.js';
import './tab_discard_exception_entry.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrScrollableMixin} from 'chrome://resources/cr_elements/cr_scrollable_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsMixin} from '../prefs/prefs_mixin.js';

import {getTemplate} from './tab_discard_exception_list.html.js';

export interface TabDiscardExceptionListElement {
  $: {
    container: HTMLElement,
    menu: CrActionMenuElement,
    noSitesAdded: HTMLElement,
  };
}

const TabDiscardExceptionListElementBase =
    CrScrollableMixin(ListPropertyUpdateMixin(PrefsMixin(PolymerElement)));

const TAB_DISCARD_EXCEPTIONS_PREF =
    'performance_tuning.tab_discarding.exceptions';

export class TabDiscardExceptionListElement extends
    TabDiscardExceptionListElementBase {
  static get is() {
    return 'tab-discard-exception-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      lastFocused_: Object,
      listBlurred_: Boolean,

      siteList_: {
        type: Array,
        value: [],
      },

      selectedSite_: String,
    };
  }

  private lastFocused_: HTMLElement|null;
  private listBlurred_: boolean;
  private siteList_: string[];
  private selectedSite_: string;

  static get observers() {
    return [
      `onPrefsChanged_(prefs.${TAB_DISCARD_EXCEPTIONS_PREF}.value.*)`,
      'onSiteListChanged_(siteList_.*)',
    ];
  }

  private hasSites_(): boolean {
    return this.siteList_.length > 0;
  }

  private onMenuClick_(e: CustomEvent<{target: HTMLElement, site: string}>) {
    this.selectedSite_ = e.detail.site;
    this.$.menu.showAt(e.detail.target);
  }

  private onEditClick_() {
    this.$.menu.close();
  }

  private onDeleteClick_() {
    this.deletePrefListItem(TAB_DISCARD_EXCEPTIONS_PREF, this.selectedSite_);
    this.$.menu.close();
  }

  private onPrefsChanged_() {
    const {value} = this.getPref(TAB_DISCARD_EXCEPTIONS_PREF);

    // Optimizes updates by keeping existing references and minimizes splices
    this.updateList('siteList_', x => x, value);
  }

  private onSiteListChanged_() {
    // This will fire an iron-resize event causing the list to resize
    this.updateScrollableContents();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-list': TabDiscardExceptionListElement;
  }
}

customElements.define(
    TabDiscardExceptionListElement.is, TabDiscardExceptionListElement);
