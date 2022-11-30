// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../settings_shared.css.js';
import './tab_discard_exception_dialog.js';
import './tab_discard_exception_entry.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {CrScrollableMixin} from 'chrome://resources/cr_elements/cr_scrollable_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsMixin} from '../prefs/prefs_mixin.js';

import {HighEfficiencyModeExceptionListAction, PerformanceMetricsProxy, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {getTemplate} from './tab_discard_exception_list.html.js';

export interface TabDiscardExceptionListElement {
  $: {
    addButton: CrButtonElement,
    list: IronListElement,
    menu: CrLazyRenderElement<CrActionMenuElement>,
    noSitesAdded: HTMLElement,
  };
}

const TabDiscardExceptionListElementBase =
    CrScrollableMixin(ListPropertyUpdateMixin(PrefsMixin(PolymerElement)));

export const TAB_DISCARD_EXCEPTIONS_PREF =
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

      /**
       * Rule corresponding to the last more actions menu opened. Indicates to
       * this element and its dialog which rule to edit or if a new one should
       * be added.
       */
      selectedRule_: {
        type: String,
        value: '',
      },

      showDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      `onPrefsChanged_(prefs.${TAB_DISCARD_EXCEPTIONS_PREF}.value.*)`,
      'onSiteListChanged_(siteList_.*)',
    ];
  }

  private lastFocused_: HTMLElement|null;
  private listBlurred_: boolean;
  private siteList_: string[];
  private selectedRule_: string;
  private showDialog_: boolean;

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private hasSites_(): boolean {
    return this.siteList_.length > 0;
  }

  private onAddClick_() {
    this.selectedRule_ = '';
    this.showDialog_ = true;
  }

  private onMenuClick_(e: CustomEvent<{target: HTMLElement, site: string}>) {
    this.selectedRule_ = e.detail.site;
    this.$.menu.get().showAt(e.detail.target);
  }

  private onEditClick_() {
    assert(this.selectedRule_);
    this.showDialog_ = true;
    this.$.menu.get().close();
  }

  private onDialogClose_() {
    this.showDialog_ = false;
  }

  private onDialogSubmit_(e: CustomEvent<string>) {
    const newRule = e.detail;
    if (this.selectedRule_) {
      // edit dialog
      if (newRule !== this.selectedRule_) {
        if (this.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value.includes(newRule)) {
          // delete instead of update, otherwise there would be a duplicate
          this.deletePrefListItem(
              TAB_DISCARD_EXCEPTIONS_PREF, this.selectedRule_);
        } else {
          this.updatePrefListItem(
              TAB_DISCARD_EXCEPTIONS_PREF, this.selectedRule_, newRule);
        }
      }
      this.metricsProxy_.recordExceptionListAction(
          HighEfficiencyModeExceptionListAction.EDIT);
      return;
    }
    // add dialog
    this.appendPrefListItem(TAB_DISCARD_EXCEPTIONS_PREF, newRule);
    this.metricsProxy_.recordExceptionListAction(
        HighEfficiencyModeExceptionListAction.ADD);
  }

  private onDeleteClick_() {
    this.deletePrefListItem(TAB_DISCARD_EXCEPTIONS_PREF, this.selectedRule_);
    this.metricsProxy_.recordExceptionListAction(
        HighEfficiencyModeExceptionListAction.REMOVE);
    this.$.menu.get().close();
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
