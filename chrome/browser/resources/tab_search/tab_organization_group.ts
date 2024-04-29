// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './strings.m.js';
import './tab_organization_new_badge.js';
import './tab_organization_results_actions.js';
import './tab_organization_shared_style.css.js';
import './tab_search_item.js';

import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {normalizeURL, TabData, TabItemType} from './tab_data.js';
import {getTemplate} from './tab_organization_group.html.js';
import type {Tab} from './tab_search.mojom-webui.js';
import type {TabSearchItem} from './tab_search_item.js';

export interface TabOrganizationGroupElement {
  $: {
    selector: IronSelectorElement,
  };
}

export class TabOrganizationGroupElement extends PolymerElement {
  static get is() {
    return 'tab-organization-group';
  }

  static get properties() {
    return {
      tabs: {
        type: Array,
        observer: 'onTabsChange_',
      },

      firstNewTabIndex: {
        type: Number,
        value: 0,
      },

      name: {
        type: String,
        value: '',
      },

      multiTabOrganization: {
        type: Boolean,
        value: false,
      },

      organizationId: {
        type: Number,
        value: -1,
      },

      showReject: {
        type: Boolean,
        value: false,
      },

      lastFocusedIndex_: {
        type: Number,
        value: 0,
      },

      showInput_: {
        type: Boolean,
        value: false,
      },

      tabDatas_: {
        type: Array,
        value: () => [],
        computed: 'computeTabDatas_(tabs.*)',
      },
    };
  }

  tabs: Tab[];
  firstNewTabIndex: number;
  name: string;
  multiTabOrganization: boolean;
  organizationId: number;
  showReject: boolean;

  private lastFocusedIndex_: number;
  private showInput_: boolean;
  private tabDatas_: TabData[];

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.showInput_ = !this.multiTabOrganization;
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

  private computeTabDatas_() {
    return this.tabs.map(
        tab => new TabData(
            tab, TabItemType.OPEN_TAB,
            new URL(normalizeURL(tab.url.url)).hostname));
  }

  private onTabsChange_() {
    if (this.lastFocusedIndex_ > this.tabs.length - 1) {
      this.lastFocusedIndex_ = 0;
    }
  }

  private getTabIndex_(index: number): number {
    return index === this.lastFocusedIndex_ ? 0 : -1;
  }

  private getInputAriaLabel_() {
    return loadTimeData.getStringF('inputAriaLabel', this.name);
  }

  private getEditButtonAriaLabel_() {
    return loadTimeData.getStringF('editAriaLabel', this.name);
  }

  private getRejectButtonAriaLabel_() {
    return loadTimeData.getStringF('rejectAriaLabel', this.name);
  }

  private showNewTabSectionHeader_(index: number) {
    return loadTimeData.getBoolean('tabReorganizationDividerEnabled') &&
        this.firstNewTabIndex > 0 && this.firstNewTabIndex === index;
  }

  private onInputFocus_() {
    const input = this.getInput_();
    if (input) {
      input.select();
    }
  }

  private onInputBlur_() {
    if (this.multiTabOrganization) {
      this.showInput_ = false;
    }
  }

  private onInputKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      if (this.multiTabOrganization) {
        this.showInput_ = false;
      } else {
        this.getInput_()!.blur();
      }
    }
  }

  private onListKeyDown_(event: KeyboardEvent) {
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

  private onSelectedChanged_() {
    if (this.$.selector.selectedItem) {
      const selectedItem = this.$.selector.selectedItem as TabSearchItem;
      const selectedItemCloseButton =
          selectedItem.shadowRoot!.querySelector(`cr-icon-button`)!;
      selectedItemCloseButton.focus();
      this.lastFocusedIndex_ = this.$.selector.indexOf(selectedItem);
    }
  }

  private onTabRemove_(event: DomRepeatEvent<TabData>) {
    const index = this.tabDatas_.indexOf(event.model.item);
    const tab = this.tabs[index];
    this.dispatchEvent(new CustomEvent('remove-tab', {
      bubbles: true,
      composed: true,
      detail: {organizationId: this.organizationId, tab: tab},
    }));
  }

  private onTabFocus_(event: DomRepeatEvent<TabData>) {
    // Ensure that when a TabSearchItem receives focus, it becomes the selected
    // item in the list.
    this.$.selector.selected = event.model.index;
  }

  private onTabBlur_(_event: DomRepeatEvent<TabData>) {
    // Ensure the selector deselects its current selection on blur. If
    // selection should move to another element in the list, this will be done
    // in onTabFocus_.
    this.$.selector.selectIndex(-1);
  }

  private onEditClick_() {
    this.showInput_ = true;
  }

  private onRejectGroupClick_(event: CustomEvent) {
    event.stopPropagation();
    event.preventDefault();
    this.dispatchEvent(new CustomEvent('reject-click', {
      bubbles: true,
      composed: true,
      detail: {organizationId: this.organizationId},
    }));
  }

  private onCreateGroupClick_(event: CustomEvent) {
    event.stopPropagation();
    event.preventDefault();
    this.dispatchEvent(new CustomEvent('create-group-click', {
      bubbles: true,
      composed: true,
      detail: {
        organizationId: this.organizationId,
        name: this.name,
        tabs: this.tabs,
      },
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-group': TabOrganizationGroupElement;
  }
}

customElements.define(
    TabOrganizationGroupElement.is, TabOrganizationGroupElement);
