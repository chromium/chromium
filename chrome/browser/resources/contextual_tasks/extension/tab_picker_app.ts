// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';
import 'chrome://resources/cr_components/composebox/composebox_tab_favicon.js';
import 'chrome://resources/cr_components/composebox/icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './tab_picker_app.css.js';
import {getHtml} from './tab_picker_app.html.js';
import {TabPickerBrowserProxyImpl} from './tab_picker_browser_proxy.js';
import type {TabPickerBrowserProxy} from './tab_picker_browser_proxy.js';

export interface TabPickerAppElement {
  $: {
    shareTabsTrigger: HTMLButtonElement,
    tabMenu: CrActionMenuElement,
  };
}

const TabPickerAppElementBase = I18nMixinLit(CrLitElement);

export class TabPickerAppElement extends TabPickerAppElementBase {
  static get is() {
    return 'tab-picker-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tabSuggestions: {type: Array},
      selectedTabs: {type: Array},
      tabMenuOpen: {type: Boolean},
      recentTabId: {type: Number},
      sharingTabsText_: {type: String},
      useUnbounded: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor tabSuggestions: TabInfo[] = [];
  accessor selectedTabs: TabInfo[] = [];
  accessor tabMenuOpen: boolean = false;
  accessor recentTabId: number|null = null;
  protected accessor sharingTabsText_: string = '';
  accessor useUnbounded: boolean = true;

  private closeTimer_: number|null = null;
  private browserProxy_: TabPickerBrowserProxy =
      TabPickerBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.updateSharingTabsText_();
    this.fetchTabSuggestions_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.cancelCloseTimer_();
  }

  override firstUpdated() {
    this.initializeUnboundedMenu_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('selectedTabs')) {
      this.updateSharingTabsText_();
    }

    if (changedProperties.has('useUnbounded')) {
      this.initializeUnboundedMenu_();
    }
  }

  private async initializeUnboundedMenu_() {
    if (this.isUnboundedMenuEnabled_()) {
      await this.$.tabMenu.setUnbounded();
    }
  }

  private async fetchTabSuggestions_() {
    try {
      const {tabs} = await this.browserProxy_.getRecentTabs();
      this.tabSuggestions = tabs || [];
      if (this.tabSuggestions.length > 0) {
        this.recentTabId = this.tabSuggestions[0]!.tabId;
      }
    } catch {
      // Standalone or test environment without proxy implementation.
    }
  }

  protected isUnboundedMenuEnabled_(): boolean {
    if (loadTimeData.isInitialized() &&
        loadTimeData.valueExists('contextualTasksUnboundedMenuEnabled') &&
        !loadTimeData.getBoolean('contextualTasksUnboundedMenuEnabled')) {
      return false;
    }
    return this.useUnbounded;
  }

  protected get hasTabSuggestions_(): boolean {
    return this.tabSuggestions.length > 0;
  }

  private updateSharingTabsText_() {
    const count = this.selectedTabs.length;
    if (count === 0) {
      const hasI18n = loadTimeData.isInitialized();
      this.sharingTabsText_ =
          (hasI18n && this.i18nExists('shareTabs') && this.i18n('shareTabs')) ||
          (hasI18n && this.i18nExists('addTabs') && this.i18n('addTabs')) ||
          'Add tabs';
      return;
    }

    this.browserProxy_.getPluralString('sharingTabs', count)
        .then((s: string) => {
          this.sharingTabsText_ = s;
        })
        .catch(() => {
          this.sharingTabsText_ =
              count === 1 ? 'Sharing 1 tab' : `Sharing ${count} tabs`;
        });
  }

  protected getRecentTabsSuffix_(): string {
    const hasI18n = loadTimeData.isInitialized();
    return (hasI18n && this.i18nExists('recentTabsSuffix') &&
            this.i18n('recentTabsSuffix')) ||
        'Recent';
  }

  protected isRecentTab_(tabId: number): boolean {
    return tabId === this.recentTabId;
  }

  protected isTabSelected_(tab: TabInfo): boolean {
    return this.selectedTabs.some(t => t.tabId === tab.tabId);
  }

  protected onShareTabsRowPointerenter_() {
    this.cancelCloseTimer_();
    this.openTabMenu_();
  }

  protected onShareTabsRowPointerleave_() {
    this.scheduleCloseTimer_();
  }

  protected onShareTabsRowClick_() {
    if (this.$.tabMenu.open) {
      this.$.tabMenu.close();
    } else {
      this.openTabMenu_();
    }
  }

  protected onShareTabsRowKeydown_(e: KeyboardEvent) {
    if (e.key === 'ArrowRight' || e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.openTabMenu_();
    }
  }

  protected onMenuPointerenter_() {
    this.cancelCloseTimer_();
  }

  protected onMenuPointerleave_() {
    this.scheduleCloseTimer_();
  }

  protected onMenuClose_() {
    this.tabMenuOpen = false;
  }

  protected onTabClick_(e: Event) {
    e.stopPropagation();
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const tab = this.tabSuggestions[index];
    if (!tab) {
      return;
    }
    const isSelected = this.isTabSelected_(tab);
    if (isSelected) {
      this.selectedTabs = this.selectedTabs.filter(t => t.tabId !== tab.tabId);
      this.browserProxy_.deleteTabContext(tab.tabId);
    } else {
      this.selectedTabs = [...this.selectedTabs, tab];
      this.browserProxy_.addTabContext(tab.tabId);
    }
    this.fire('tab-selected', {tab, selected: !isSelected});
  }

  private async openTabMenu_() {
    if (!this.hasTabSuggestions_ || this.$.tabMenu.open) {
      return;
    }
    if (this.isUnboundedMenuEnabled_() &&
        !this.$.tabMenu.getDialog().hasAttribute('unbounded')) {
      await this.$.tabMenu.setUnbounded();
    }
    this.tabMenuOpen = true;
    this.$.tabMenu.showAt(this.$.shareTabsTrigger, {
      anchorAlignmentX: AnchorAlignment.AFTER_END,
      anchorAlignmentY: AnchorAlignment.AFTER_START,
      noOffset: true,
    });
  }

  private scheduleCloseTimer_() {
    this.cancelCloseTimer_();
    this.closeTimer_ = window.setTimeout(() => {
      if (this.$.tabMenu.open) {
        this.$.tabMenu.close();
      }
    }, 300);
  }

  private cancelCloseTimer_() {
    if (this.closeTimer_ !== null) {
      window.clearTimeout(this.closeTimer_);
      this.closeTimer_ = null;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-picker-app': TabPickerAppElement;
  }
}

customElements.define(TabPickerAppElement.is, TabPickerAppElement);
