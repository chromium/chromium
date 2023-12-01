// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './tab_organization_shared_style.css.js';
import './tab_search_item.js';

import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TabData, TabItemType} from './tab_data.js';
import {getTemplate} from './tab_organization_results.html.js';
import {Tab} from './tab_search.mojom-webui.js';
import {TabSearchItem} from './tab_search_item.js';

const MINIMUM_SCROLLABLE_MAX_HEIGHT: number = 204;
const NON_SCROLLABLE_VERTICAL_SPACING: number = 120;

export interface TabOrganizationResultsElement {
  $: {
    input: CrInputElement,
    scrollable: HTMLElement,
    selector: IronSelectorElement,
  };
}

export class TabOrganizationResultsElement extends PolymerElement {
  static get is() {
    return 'tab-organization-results';
  }

  static get properties() {
    return {
      tabs: {
        type: Array,
        observer: 'onTabsChange_',
      },

      name: String,

      availableHeight: {
        type: Number,
        observer: 'onAvailableHeightChange_',
      },

      lastFocusedIndex_: {
        type: Number,
        value: 0,
      },

      tabDatas_: {
        type: Array,
        value: () => [],
        computed: 'computeTabDatas_(tabs.*)',
      },
    };
  }

  tabs: Tab[];
  name: string;
  availableHeight: number;

  private lastFocusedIndex_: number;
  private tabDatas_: TabData[];

  static get template() {
    return getTemplate();
  }

  private computeTabDatas_() {
    return this.tabs.map(
        tab => new TabData(
            tab, TabItemType.OPEN_TAB, new URL(tab.url.url).hostname));
  }

  private onTabsChange_() {
    if (this.lastFocusedIndex_ > this.tabs.length - 1) {
      this.lastFocusedIndex_ = 0;
    }
  }

  private getTabIndex_(index: number): number {
    return index === this.lastFocusedIndex_ ? 0 : -1;
  }

  private onAvailableHeightChange_() {
    const maxHeight = Math.max(
        MINIMUM_SCROLLABLE_MAX_HEIGHT,
        (this.availableHeight - NON_SCROLLABLE_VERTICAL_SPACING));
    this.$.scrollable.style.maxHeight = maxHeight + 'px';
  }

  private onInputFocus_() {
    this.$.input.select();
  }

  private onInputKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      this.$.input.blur();
    }
  }

  private onListKeyDown_(event: KeyboardEvent) {
    if (event.shiftKey) {
      return;
    }

    const selector = this.$.selector;
    if (selector.selected === undefined) {
      return;
    }

    if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
      if (event.key === 'ArrowUp') {
        selector.selectPrevious();
      } else {
        selector.selectNext();
      }
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
      detail: {tab},
    }));
  }

  private onTabFocus_(event: DomRepeatEvent<TabData>) {
    // Ensure that when a TabSearchItem receives focus, it becomes the selected
    // item in the list.
    this.$.selector.selected = event.model.index;
  }

  private onCreateGroupClick_() {
    this.dispatchEvent(new CustomEvent('create-group-click', {
      bubbles: true,
      composed: true,
      detail: {name: this.name, tabs: this.tabs},
    }));
  }

  private onLearnMoreClick_() {
    this.dispatchEvent(new CustomEvent('learn-more-click', {
      bubbles: true,
      composed: true,
    }));
  }

  private onFeedbackSelectedOptionChanged_(
      event: CustomEvent<{value: CrFeedbackOption}>) {
    this.dispatchEvent(new CustomEvent('feedback', {
      bubbles: true,
      composed: true,
      detail: {value: event.detail.value},
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-results': TabOrganizationResultsElement;
  }
}

customElements.define(
    TabOrganizationResultsElement.is, TabOrganizationResultsElement);
