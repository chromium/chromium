// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '../strings.m.js';
import './auto_tab_groups_new_badge.js';
import './auto_tab_groups_results_actions.js';
import '../tab_search_item.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrPageSelectorElement} from 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {normalizeURL, TabData, TabItemType} from '../tab_data.js';
import {getCss} from './auto_tab_groups_group.css.js';
import {getHtml} from './auto_tab_groups_group.html.js';
import type {Tab} from '../tab_search.mojom-webui.js';
import type {TabSearchItemElement} from '../tab_search_item.js';

function getEventTargetIndex(e: Event): number {
  return Number((e.currentTarget as HTMLElement).dataset['index']);
}

export interface AutoTabGroupsGroupElement {
  $: {
    selector: CrPageSelectorElement,
  };
}

export class AutoTabGroupsGroupElement extends CrLitElement {
  static get is() {
    return 'auto-tab-groups-group';
  }

  static override get properties() {
    return {
      tabs: {type: Array},
      firstNewTabIndex: {type: Number},
      name: {type: String},
      multiTabOrganization: {type: Boolean},
      organizationId: {type: Number},

      showReject: {
        type: Boolean,
        reflect: true,
      },

      lastFocusedIndex_: {type: Number},
      showInput_: {type: Boolean},
      tabDatas_: {type: Array},
      changedName_: {type: Boolean},
    };
  }

  tabs: Tab[] = [];
  firstNewTabIndex: number = 0;
  name: string = '';
  multiTabOrganization: boolean = false;
  organizationId: number = -1;
  showReject: boolean = false;

  private lastFocusedIndex_: number = 0;
  protected showInput_: boolean = false;
  protected tabDatas_: TabData[] = [];
  private changedName_: boolean = false;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.showInput_ = !this.multiTabOrganization;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('tabs')) {
      if (this.lastFocusedIndex_ > this.tabs.length - 1) {
        this.lastFocusedIndex_ = 0;
      }

      this.tabDatas_ = this.computeTabDatas_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('showInput_')) {
      this.focusInput();
    }
  }

  focusInput() {
    const input = this.getInput_();
    if (input) {
      input.focus();
    }
  }

  private getInput_(): CrInputElement|null {
    if (!this.showInput_) {
      return null;
    }
    const id = this.multiTabOrganization ? '#multiOrganizationInput' :
                                           '#singleOrganizationInput';
    return this.shadowRoot!.querySelector<CrInputElement>(id);
  }

  private computeTabDatas_(): TabData[] {
    return this.tabs.map(
        tab => new TabData(
            tab, TabItemType.OPEN_TAB,
            new URL(normalizeURL(tab.url.url)).hostname));
  }

  protected getTabIndex_(index: number): number {
    return index === this.lastFocusedIndex_ ? 0 : -1;
  }

  protected getInputAriaLabel_() {
    return loadTimeData.getStringF('inputAriaLabel', this.name);
  }

  protected getEditButtonAriaLabel_() {
    return loadTimeData.getStringF('editAriaLabel', this.name);
  }

  protected getRejectButtonAriaLabel_() {
    return loadTimeData.getStringF('rejectAriaLabel', this.name);
  }

  protected showNewTabSectionHeader_(index: number): boolean {
    return loadTimeData.getBoolean('tabReorganizationDividerEnabled') &&
        this.firstNewTabIndex > 0 && this.firstNewTabIndex === index;
  }

  protected onInputFocus_() {
    const input = this.getInput_();
    if (input) {
      input.select();
    }
  }

  protected onInputBlur_() {
    if (this.multiTabOrganization && !!this.getInput_()) {
      this.showInput_ = false;
    }
    this.maybeRenameGroup_();
  }

  protected maybeRenameGroup_() {
    if (this.changedName_) {
      this.fire(
          'name-change',
          {organizationId: this.organizationId, name: this.name});
      this.changedName_ = false;
    }
  }

  protected onInputKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      if (this.multiTabOrganization) {
        this.showInput_ = false;
      } else {
        this.getInput_()!.blur();
      }
    }
  }

  protected onListKeyDown_(event: KeyboardEvent) {
    const selector = this.$.selector;
    if (selector.selected === undefined) {
      return;
    }

    let handled = false;
    if (event.shiftKey && event.key === 'Tab') {
      // Explicitly focus the element prior to the list in focus order and
      // override the default behavior, which would be to focus the row that
      // the currently focused close button is in.
      if (this.multiTabOrganization) {
        this.shadowRoot!.querySelector<CrIconButtonElement>(
                            `#rejectButton`)!.focus();
      } else {
        this.getInput_()!.focus();
      }
      handled = true;
    } else if (!event.shiftKey) {
      if (event.key === 'ArrowUp') {
        selector.selectPrevious();
        handled = true;
      } else if (event.key === 'ArrowDown') {
        selector.selectNext();
        handled = true;
      }
    }

    if (handled) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  protected onSelectedChanged_() {
    if (this.$.selector.selectedItem) {
      const selectedItem = this.$.selector.selectedItem as TabSearchItemElement;
      const selectedItemCloseButton =
          selectedItem.shadowRoot!.querySelector(`cr-icon-button`)!;
      selectedItemCloseButton.focus();
      this.lastFocusedIndex_ =
          Number.parseInt(selectedItem.dataset['index']!, 10);
    }
  }

  protected onTabRemove_(e: Event) {
    const index = getEventTargetIndex(e);
    const tab = this.tabs[index];
    this.fire('remove-tab', {organizationId: this.organizationId, tab});
  }

  protected onTabFocus_(e: Event) {
    // Ensure that when a TabSearchItemElement receives focus, it becomes the
    // selected item in the list.
    const index = getEventTargetIndex(e);
    this.$.selector.selected = index;
  }

  protected onTabBlur_(_e: Event) {
    // Ensure the selector deselects its current selection on blur. If
    // selection should move to another element in the list, this will be done
    // in onTabFocus_.
    this.$.selector.select(-1);
  }

  protected onEditClick_() {
    this.showInput_ = true;
  }

  protected onRejectGroupClick_(event: CustomEvent) {
    event.stopPropagation();
    event.preventDefault();
    this.dispatchEvent(new CustomEvent('reject-click', {
      bubbles: true,
      composed: true,
      detail: {organizationId: this.organizationId},
    }));
  }

  protected onCreateGroupClick_(event: CustomEvent) {
    event.stopPropagation();
    event.preventDefault();
    this.dispatchEvent(new CustomEvent('create-group-click', {
      bubbles: true,
      composed: true,
      detail: {
        organizationId: this.organizationId,
        tabs: this.tabs,
      },
    }));
  }

  protected onNameChanged_(e: CustomEvent<{value: string}>) {
    if (this.name !== e.detail.value) {
      this.name = e.detail.value;
      this.changedName_ = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'auto-tab-groups-group': AutoTabGroupsGroupElement;
  }
}

customElements.define(AutoTabGroupsGroupElement.is, AutoTabGroupsGroupElement);
