// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import '../settings_shared.css.js';
import './tab_discard_exception_dialog.js';
import './tab_discard_exception_entry.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {ListPropertyUpdateMixin, ListPropertyUpdateMixinInterface} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {PaperTooltipElement} from 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsMixin, PrefsMixinInterface} from '../prefs/prefs_mixin.js';
import {TooltipMixin, TooltipMixinInterface} from '../tooltip_mixin.js';

import {HighEfficiencyModeExceptionListAction, PerformanceMetricsProxy, PerformanceMetricsProxyImpl} from './performance_metrics_proxy.js';
import {TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, TAB_DISCARD_EXCEPTIONS_PREF} from './tab_discard_exception_dialog.js';
import {TabDiscardExceptionEntry} from './tab_discard_exception_entry.js';
import {getTemplate} from './tab_discard_exception_list.html.js';

export const TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE: number = 5;

export interface TabDiscardExceptionListElement {
  $: {
    addButton: CrButtonElement,
    collapse: IronCollapseElement,
    expandButton: CrExpandButtonElement,
    list: DomRepeat,
    overflowList: DomRepeat,
    menu: CrLazyRenderElement<CrActionMenuElement>,
    noSitesAdded: HTMLElement,
    tooltip: PaperTooltipElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const TabDiscardExceptionListElementBase =
    TooltipMixin(ListPropertyUpdateMixin(PrefsMixin(PolymerElement))) as
    Constructor<TooltipMixinInterface&ListPropertyUpdateMixinInterface&
                PrefsMixinInterface&PolymerElement>;

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
      siteList_: {
        type: Array,
        value: [],
      },

      overflowSiteListExpanded: {type: Boolean, value: false},

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

      tooltipText_: String,
    };
  }

  static get observers() {
    return [
      `onPrefsChanged_(prefs.${TAB_DISCARD_EXCEPTIONS_PREF}.value.*,` +
          `prefs.${TAB_DISCARD_EXCEPTIONS_MANAGED_PREF}.value.*)`,
    ];
  }

  private siteList_: TabDiscardExceptionEntry[];
  private overflowSiteListExpanded: boolean;
  private selectedRule_: string;
  private showDialog_: boolean;
  private tooltipText_: string;

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  private hasSites_(): boolean {
    return this.siteList_.length > 0;
  }

  private hasOverflowSites_() {
    return this.siteList_.length > TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE;
  }

  private getSiteList_() {
    return this.siteList_.slice(0, TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE);
  }

  private getOverflowSiteList_() {
    return this.siteList_.slice(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE);
  }

  private onAddClick_() {
    this.selectedRule_ = '';
    this.showDialog_ = true;
  }

  private onMenuClick_(e: CustomEvent<{target: HTMLElement, site: string}>) {
    e.stopPropagation();
    this.selectedRule_ = e.detail.site;
    this.$.menu.get().showAt(e.detail.target);
  }

  private onEditClick_() {
    assert(this.selectedRule_);
    this.showDialog_ = true;
    this.$.menu.get().close();
  }

  private onDeleteClick_() {
    this.deletePrefListItem(TAB_DISCARD_EXCEPTIONS_PREF, this.selectedRule_);
    this.metricsProxy_.recordExceptionListAction(
        HighEfficiencyModeExceptionListAction.REMOVE);
    this.$.menu.get().close();
  }

  private onDialogClose_() {
    this.showDialog_ = false;
  }

  private onAddException_() {
    this.overflowSiteListExpanded = true;
  }

  private onPrefsChanged_() {
    const newSites: TabDiscardExceptionEntry[] = [];
    for (const pref
             of [TAB_DISCARD_EXCEPTIONS_MANAGED_PREF,
                 TAB_DISCARD_EXCEPTIONS_PREF]) {
      // Annotate sites with their managed status and append them to newSites
      // with managed sites first.
      const {value: sites, enforcement} = this.getPref(pref);
      const siteToExceptionEntry = (site: string) => ({
        site,
        managed: enforcement === chrome.settingsPrivate.Enforcement.ENFORCED,
      });
      newSites.push(...sites.map(siteToExceptionEntry));
    }

    // Optimizes updates by keeping existing references and minimizes splices
    this.updateList(
        'siteList_', (entry: TabDiscardExceptionEntry) => entry.site, newSites);
  }

  /**
   * Need to use common tooltip since the tooltip in the entry is cut off from
   * the iron-list.
   */
  private onShowTooltip_(e: CustomEvent<{target: HTMLElement, text: string}>) {
    this.tooltipText_ = e.detail.text;
    this.showTooltipAtTarget(this.$.tooltip, e.detail.target);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-discard-exception-list': TabDiscardExceptionListElement;
  }
}

customElements.define(
    TabDiscardExceptionListElement.is, TabDiscardExceptionListElement);
