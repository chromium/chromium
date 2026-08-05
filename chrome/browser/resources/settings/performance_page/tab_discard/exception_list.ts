// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import './exception_edit_dialog.js';
import './exception_entry.js';
import './exception_tabbed_add_dialog.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {TooltipMixinLit} from '../../tooltip_mixin_lit.js';
import type {PerformanceMetricsProxy} from '../performance_metrics_proxy.js';
import {MemorySaverModeExceptionListAction, PerformanceMetricsProxyImpl} from '../performance_metrics_proxy.js';

import type {ExceptionEntry} from './exception_entry.js';
import {getCss} from './exception_list.css.js';
import {getHtml} from './exception_list.html.js';
import {TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, TAB_DISCARD_EXCEPTIONS_PREF} from './exception_validation_mixin.js';

export const TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE: number = 5;

export interface ExceptionListElement {
  $: {
    addButton: CrButtonElement,
    collapse: CrCollapseElement,
    expandButton: CrExpandButtonElement,
    menu: CrLazyRenderLitElement<CrActionMenuElement>,
    noSitesAdded: HTMLElement,
    tooltip: CrTooltipElement,
  };
}

const ExceptionListElementBase =
    TooltipMixinLit(PrefServiceObserverMixinLit(CrLitElement));

export class ExceptionListElement extends
    ExceptionListElementBase {
  static get is() {
    return 'tab-discard-exception-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      siteList_: {type: Array},
      overflowSiteListExpanded_: {type: Boolean},
      selectedRule_: {type: String},
      showTabbedAddDialog_: {type: Boolean},
      showEditDialog_: {type: Boolean},
      tooltipText_: {type: String},
    };
  }

  protected accessor siteList_: ExceptionEntry[] = [];
  protected accessor overflowSiteListExpanded_: boolean = false;
  protected accessor selectedRule_: string = '';
  protected accessor showTabbedAddDialog_: boolean = false;
  protected accessor showEditDialog_: boolean = false;
  protected accessor tooltipText_: string = '';

  private metricsProxy_: PerformanceMetricsProxy =
      PerformanceMetricsProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addPrefObserver(TAB_DISCARD_EXCEPTIONS_PREF, () => this.updateList_());
    this.addPrefObserver(
        TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, () => this.updateList_());
  }

  protected hasSites_(): boolean {
    return this.siteList_.length > 0;
  }

  protected hasOverflowSites_(): boolean {
    return this.siteList_.length > TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE;
  }

  protected getSiteList_() {
    return this.siteList_.slice(-TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE)
        .reverse();
  }

  protected getOverflowSiteList_() {
    return this.siteList_.slice(0, -TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE)
        .reverse();
  }

  protected onOverflowSiteListExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.overflowSiteListExpanded_ = e.detail.value;
  }

  protected onAddClick_() {
    assert(!this.showEditDialog_);
    this.showTabbedAddDialog_ = true;
  }

  protected onMenuClick_(e: CustomEvent<{target: HTMLElement, site: string}>) {
    e.stopPropagation();
    this.selectedRule_ = e.detail.site;
    this.$.menu.get().showAt(e.detail.target);
  }

  protected onEditClick_() {
    assert(this.selectedRule_);
    assert(!this.showTabbedAddDialog_);
    this.showEditDialog_ = true;
    this.$.menu.get().close();
  }

  protected onDeleteClick_() {
    PrefService.getInstance().deletePrefDictEntry(
        TAB_DISCARD_EXCEPTIONS_PREF, this.selectedRule_);
    this.metricsProxy_.recordExceptionListAction(
        MemorySaverModeExceptionListAction.REMOVE);
    this.$.menu.get().close();
  }

  protected onTabbedAddDialogClose_() {
    this.showTabbedAddDialog_ = false;
  }

  protected onEditDialogClose_() {
    this.showEditDialog_ = false;
  }

  private updateList_() {
    const newSites: ExceptionEntry[] = [];

    const siteToExceptionEntry =
        (site: string, prefObject: chrome.settingsPrivate.PrefObject) => ({
          site,
          managed: prefObject.enforcement ===
              chrome.settingsPrivate.Enforcement.ENFORCED,
        });

    {
      const prefObject = PrefService.getInstance().getPref<string[]>(
          TAB_DISCARD_EXCEPTIONS_MANAGED_PREF);
      const sites: string[] = prefObject.value;
      newSites.push(
          ...sites.map(site => siteToExceptionEntry(site, prefObject)));
    }

    {
      const prefObject =
          PrefService.getInstance().getPref<Record<string, string>>(
              TAB_DISCARD_EXCEPTIONS_PREF);
      const sites: string[] = Object.keys(prefObject.value);
      newSites.push(
          ...sites.map(site => siteToExceptionEntry(site, prefObject)));
    }

    this.siteList_ = newSites;
  }

  /**
   * Need to use common tooltip since the tooltip in the entry is cut off from
   * the iron-list.
   */
  protected async onShowTooltip_(
      e: CustomEvent<{target: HTMLElement, text: string}>) {
    this.tooltipText_ = e.detail.text;
    await this.updateComplete;
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
