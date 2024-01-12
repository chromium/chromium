// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './strings.m.js';
import './tab_organization_shared_style.css.js';
import './tab_search_item.js';

import {CrFeedbackButtonsElement, CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
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
    feedbackButtons: CrFeedbackButtonsElement,
    header: HTMLElement,
    input: CrInputElement,
    learnMore: HTMLElement,
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

      isLastOrganization: Boolean,

      lastFocusedIndex_: {
        type: Number,
        value: 0,
      },

      showRefresh_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('tabOrganizationRefreshButtonEnabled'),
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
  isLastOrganization: boolean;

  private lastFocusedIndex_: number;
  private showRefresh_: boolean;
  private tabDatas_: TabData[];

  static get template() {
    return getTemplate();
  }

  announceHeader() {
    this.$.header.textContent = '';
    this.$.header.textContent = this.getTitle_();
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

  private getTitle_(): string {
    return loadTimeData.getString('successTitle');
  }

  private getRefreshButtonText_(): string {
    if (this.isLastOrganization) {
      return loadTimeData.getString('rejectFinalSuggestion');
    }
    return loadTimeData.getString('rejectSuggestion');
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

  private getInputAriaLabel_() {
    return loadTimeData.getStringF('inputAriaLabel', this.name);
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

  private onTabBlur_(_event: DomRepeatEvent<TabData>) {
    // Ensure the selector deselects its current selection on blur. If
    // selection should move to another element in the list, this will be done
    // in onTabFocus_.
    this.$.selector.selectIndex(-1);
  }

  private onRefreshClick_() {
    this.dispatchEvent(new CustomEvent('refresh-click', {
      bubbles: true,
      composed: true,
    }));
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

  private onLearnMoreKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      this.onLearnMoreClick_();
    }
  }

  private onFeedbackKeyDown_(event: KeyboardEvent) {
    if ((event.key !== 'ArrowLeft' && event.key !== 'ArrowRight')) {
      return;
    }
    const feedbackButtons =
        this.$.feedbackButtons.shadowRoot!.querySelectorAll(`cr-icon-button`);
    const focusableElements = [
      this.$.learnMore,
      feedbackButtons[0]!,
      feedbackButtons[1]!,
    ];
    const focusableElementCount = focusableElements.length;
    const focusedIndex =
        focusableElements.findIndex((element) => element.matches(':focus'));
    if (focusedIndex < 0) {
      return;
    }
    let nextFocusedIndex = 0;
    if (event.key === 'ArrowLeft') {
      nextFocusedIndex =
          (focusedIndex + focusableElementCount - 1) % focusableElementCount;
    } else if (event.key === 'ArrowRight') {
      nextFocusedIndex = (focusedIndex + 1) % focusableElementCount;
    }
    focusableElements[nextFocusedIndex]!.focus();
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
