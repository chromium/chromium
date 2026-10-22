// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';
import 'chrome://resources/cr_components/composebox/composebox_tab_favicon.js';
import 'chrome://resources/cr_components/composebox/icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {ExtensionBrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {recordTabPickerTabSelected, TabPickerSurface} from 'chrome://resources/cr_components/composebox/common.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './tab_picker_app.css.js';
import {getHtml} from './tab_picker_app.html.js';
import {TabPickerBrowserProxyImpl} from './tab_picker_browser_proxy.js';
import type {TabPickerBrowserProxy} from './tab_picker_browser_proxy.js';

const CLOSE_MENU_DELAY_MS = 50;

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
      darkMode: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor tabSuggestions: TabInfo[] = [];
  accessor selectedTabs: TabInfo[] = [];
  accessor tabMenuOpen: boolean = false;
  accessor recentTabId: number|null = null;
  accessor useUnbounded: boolean = true;
  accessor darkMode: boolean = false;

  protected accessor sharingTabsText_: string = '';

  private closeTimer_: number|null = null;
  private pointerDownOnTrigger_: boolean = false;
  private shouldOpenMenu_: boolean = false;
  // Tracks the latest open request so an in-flight async fetch does not open
  // the menu after the pointer leaves the trigger or the menu is closed.
  private openMenuRequestId_: number = 0;
  private listenerIds_: number[] = [];
  private browserProxy_: TabPickerBrowserProxy =
      TabPickerBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.setFocusOutlineVisible_(false);
    this.darkMode = this.isDarkModeEnabled_();
    this.updateSharingTabsText_();
    this.fetchTabSuggestions_();

    const callbackRouter =
        ExtensionBrowserProxyImpl.getInstance().callbackRouter;
    this.listenerIds_.push(
        callbackRouter.onTabContextUpdated.addListener((tabs: TabInfo[]) => {
          this.selectedTabs = tabs;
        }));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    ++this.openMenuRequestId_;
    this.cancelCloseTimer_();
    const callbackRouter =
        ExtensionBrowserProxyImpl.getInstance().callbackRouter;
    for (const id of this.listenerIds_) {
      callbackRouter.removeListener(id);
    }
    this.listenerIds_ = [];
  }

  override firstUpdated() {
    this.$.tabMenu.addEventListener(
        'keydown', this.onMenuKeydown_.bind(this), true);
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
      this.recentTabId =
          this.tabSuggestions.length > 0 ? this.tabSuggestions[0]!.tabId : null;
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

  protected isDarkModeEnabled_(): boolean {
    const params = new URLSearchParams(window.location.search);
    return params.get('cs') === '1';
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

  // FocusOutlineManager defaults to visible=true and only listens to keydown
  // and mousedown (not pointerenter/pointermove), while cr-action-menu checks
  // FocusOutlineManager.visible on open and calls .focus() on mouseover.
  // Explicitly sync outline visibility on pointer vs. keyboard interactions.
  private setFocusOutlineVisible_(visible: boolean) {
    FocusOutlineManager.forDocument(document).visible = visible;
  }

  protected onShareTabsRowPointerenter_() {
    this.pointerDownOnTrigger_ = false;
    this.setFocusOutlineVisible_(false);
    this.shouldOpenMenu_ = true;
    this.cancelCloseTimer_();
    this.openTabMenu_();
  }

  protected onShareTabsRowPointermove_() {
    this.setFocusOutlineVisible_(false);
  }

  protected onShareTabsRowPointerdown_() {
    this.pointerDownOnTrigger_ = true;
    this.setFocusOutlineVisible_(false);
  }

  protected onShareTabsRowPointerleave_() {
    this.pointerDownOnTrigger_ = false;
    this.shouldOpenMenu_ = false;
    ++this.openMenuRequestId_;
    this.$.shareTabsTrigger.blur();
    this.scheduleCloseTimer_();
  }

  protected onShareTabsRowFocus_() {
    if (!this.tabMenuOpen && !this.pointerDownOnTrigger_) {
      this.setFocusOutlineVisible_(true);
    }
  }

  protected onShareTabsRowClick_() {
    this.setFocusOutlineVisible_(false);
    this.cancelCloseTimer_();
    if (this.$.tabMenu.open) {
      this.shouldOpenMenu_ = false;
      ++this.openMenuRequestId_;
      this.$.tabMenu.close();
    } else {
      this.shouldOpenMenu_ = true;
      this.openTabMenu_();
    }
    this.pointerDownOnTrigger_ = false;
  }

  protected onShareTabsRowKeydown_(e: KeyboardEvent) {
    this.setFocusOutlineVisible_(true);
    if (e.key === 'ArrowRight' || e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.shouldOpenMenu_ = true;
      this.cancelCloseTimer_();
      this.openTabMenu_(/*focusFirstItem=*/ true);
    }
  }

  protected onMenuKeydown_(e: KeyboardEvent) {
    this.setFocusOutlineVisible_(true);
    if (e.key === 'ArrowLeft' || e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      e.stopImmediatePropagation();
      this.$.tabMenu.close();
      this.$.shareTabsTrigger.focus();
      return;
    }

    if (e.key === 'Tab' || e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      const items = Array.from(this.$.tabMenu.querySelectorAll<HTMLElement>(
          'button.dropdown-item:not([disabled])'));
      if (items.length === 0) {
        return;
      }

      e.preventDefault();
      e.stopPropagation();
      e.stopImmediatePropagation();

      const focused = this.shadowRoot.activeElement as HTMLElement | null;
      const index = focused ? items.indexOf(focused) : -1;

      const next = e.key === 'ArrowDown' || (e.key === 'Tab' && !e.shiftKey);
      const numOptions = items.length;
      const nextIndex = index === -1 ?
          (next ? 0 : numOptions - 1) :
          (numOptions + index + (next ? 1 : -1)) % numOptions;
      items[nextIndex]!.focus({preventScroll: this.isUnboundedMenuEnabled_()});
    }
  }

  protected async onMenuPointerenter_() {
    this.cancelCloseTimer_();
    await this.fetchTabSuggestions_();
    // If all eligible tabs were closed while the menu remained open, close it
    // so an empty modal dialog is not left on screen.
    if (!this.hasTabSuggestions_ && this.$.tabMenu.open) {
      this.$.tabMenu.close();
    }
  }

  protected onMenuPointermove_() {
    this.setFocusOutlineVisible_(false);
  }

  protected onMenuPointerleave_() {
    this.scheduleCloseTimer_();
  }

  protected onMenuClose_() {
    this.shouldOpenMenu_ = false;
    ++this.openMenuRequestId_;
    this.tabMenuOpen = false;
    if (!FocusOutlineManager.forDocument(document).visible) {
      this.$.shareTabsTrigger.blur();
    }
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
      this.selectedTabs = [tab, ...this.selectedTabs];
      this.browserProxy_.addTabContext(tab.tabId);
      const hasActiveTab =
          this.tabSuggestions.some(t => t.showInCurrentTabChip);
      recordTabPickerTabSelected(
          TabPickerSurface.CONTEXTUAL_TASKS_EXTENSION, tab.showInCurrentTabChip,
          index, hasActiveTab);
    }
    this.fire('tab-selected', {tab, selected: !isSelected});
  }

  private async openTabMenu_(focusFirstItem: boolean = false) {
    const wasOpen = this.$.tabMenu.open;
    const requestId = ++this.openMenuRequestId_;
    await this.fetchTabSuggestions_();
    await this.updateComplete;
    if (!this.hasTabSuggestions_) {
      // `openTabMenu_()` can be triggered on hover while `#tabMenu` is already
      // open; if all eligible tabs were closed in the meantime, close the
      // already-open menu so an empty modal dialog does not stay visible.
      if (this.$.tabMenu.open) {
        this.$.tabMenu.close();
      }
      return;
    }
    if (wasOpen || !this.shouldOpenMenu_ || this.$.tabMenu.open ||
        requestId !== this.openMenuRequestId_ || !this.isConnected) {
      return;
    }
    if (this.isUnboundedMenuEnabled_() &&
        !this.$.tabMenu.getDialog().hasAttribute('unbounded')) {
      await this.$.tabMenu.setUnbounded();
      // Abort if the pointer left or the menu state changed while awaiting
      // setUnbounded().
      if (!this.shouldOpenMenu_ || this.$.tabMenu.open ||
          requestId !== this.openMenuRequestId_ || !this.isConnected) {
        return;
      }
    }
    this.tabMenuOpen = true;
    this.$.tabMenu.showAt(this.$.shareTabsTrigger, {
      anchorAlignmentX: AnchorAlignment.AFTER_END,
      anchorAlignmentY: AnchorAlignment.AFTER_START,
      noOffset: true,
    });
    if (focusFirstItem) {
      this.focusFirstTabItem_();
      requestAnimationFrame(() => {
        if (this.$.tabMenu.open) {
          this.focusFirstTabItem_();
        }
      });
    }
  }

  private focusFirstTabItem_() {
    const firstItem = this.$.tabMenu.querySelector<HTMLElement>(
        'button.dropdown-item:not([disabled])');
    firstItem?.focus({preventScroll: this.isUnboundedMenuEnabled_()});
  }

  private scheduleCloseTimer_() {
    this.cancelCloseTimer_();
    this.closeTimer_ = window.setTimeout(() => {
      this.closeTimer_ = null;
      if (this.$.tabMenu.open) {
        this.$.tabMenu.close();
        this.$.shareTabsTrigger.blur();
      }
    }, CLOSE_MENU_DELAY_MS);
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
