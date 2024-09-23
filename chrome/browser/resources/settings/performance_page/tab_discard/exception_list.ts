// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../settings_shared.css.js';
import './exception_edit_dialog.js';
import './exception_entry.js';
import './exception_tabbed_add_dialog.js';

import type {PrefsMixinInterface} from '/shared/settings/prefs/prefs_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import type {ListPropertyUpdateMixinInterface} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {DomRepeat} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {TooltipMixinInterface} from '../../tooltip_mixin.js';
import {TooltipMixin} from '../../tooltip_mixin.js';
import type {PerformanceMetricsProxy} from '../performance_metrics_proxy.js';
import {MemorySaverModeExceptionListAction, PerformanceMetricsProxyImpl} from '../performance_metrics_proxy.js';

import type {ExceptionEntry} from './exception_entry.js';
import {getTemplate} from './exception_list.html.js';
import {TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, TAB_DISCARD_EXCEPTIONS_PREF} from './exception_validation_mixin.js';

export const TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE: number = 5;

export interface ExceptionListElement {
  $: {
    addButton: CrButtonElement,
    collapse: CrCollapseElement,
    expandButton: CrExpandButtonElement,
    list: DomRepeat,
    overflowList: DomRepeat,
    menu: CrLazyRenderElement<CrActionMenuElement>,
    noSitesAdded: HTMLElement,
    tooltip: CrTooltipElement,
  };
}

type Constructor<T> = new (...args: any[]) => T;
const ExceptionListElementBase =
    TooltipMixin(ListPropertyUpdateMixin(PrefsMixin(PolymerElement))) as
    Constructor<TooltipMixinInterface&ListPropertyUpdateMixinInterface&
                PrefsMixinInterface&PolymerElement>;

export class ExceptionListElement extends
    ExceptionListElementBase {
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

      showTabbedAddDialog_: {
        type: Boolean,
        value: false,
      },

      showEditDialog_: {
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

  private siteList_: ExceptionEntry[];
  private overflowSiteListExpanded: boolean;
  private selectedRule_: string;
  private showTabbedAddDialog_: boolean;
  private showEditDialog_: boolean;
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
    return this.siteList_.slice(-TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE)
        .reverse();
  }

  private getOverflowSiteList_() {
    return this.siteList_.slice(0, -TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE)
        .reverse();
  }

  private onAddClick_() {
    assert(!this.showEditDialog_);
    this.showTabbedAddDialog_ = true;
  }

  private onMenuClick_(e: CustomEvent<{target: HTMLElement, site: string}>) {
    e.stopPropagation();
    this.selectedRule_ = e.detail.site;
    this.$.menu.get().showAt(e.detail.target);
  }

  private onEditClick_() {
    assert(this.selectedRule_);
    assert(!this.showTabbedAddDialog_);
    this.showEditDialog_ = true;
    this.$.menu.get().close();
  }

  private onDeleteClick_() {
    this.deletePrefDictEntry(TAB_DISCARD_EXCEPTIONS_PREF, this.selectedRule_);
    this.metricsProxy_.recordExceptionListAction(
        MemorySaverModeExceptionListAction.REMOVE);
    this.$.menu.get().close();
  }

  private onTabbedAddDialogClose_() {
    this.showTabbedAddDialog_ = false;
  }

  private onEditDialogClose_() {
    this.showEditDialog_ = false;
  }

  private onPrefsChanged_() {
    const newSites: ExceptionEntry[] = [];
    for (const pref
             of [TAB_DISCARD_EXCEPTIONS_MANAGED_PREF,
                 TAB_DISCARD_EXCEPTIONS_PREF]) {
      // Annotate sites with their managed status and append them to newSites
      // with managed sites first.
      const prefObject = this.getPref(pref);
      let sites = prefObject.value;

      if (sites.constructor.name === 'Object') {
        sites = Object.keys(sites);
      }
      const siteToExceptionEntry = (site: string) => ({
        site,
        managed: prefObject.enforcement ===
            chrome.settingsPrivate.Enforcement.ENFORCED,
      });
      newSites.push(...sites.map(siteToExceptionEntry));
    }

    // Optimizes updates by keeping existing references and minimizes splices
    this.updateList(
        'siteList_', (entry: ExceptionEntry) => entry.site, newSites);
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
    'tab-discard-exception-list': ExceptionListElement;
  }
}

customElements.define(
    ExceptionListElement.is, ExceptionListElement);
