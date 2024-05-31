// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import './strings.m.js';
import './tab_organization_group.js';
import './tab_organization_results_actions.js';

import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import type {CrFeedbackButtonsElement} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import type {TabOrganizationGroupElement} from './tab_organization_group.js';
import {getCss} from './tab_organization_results.css.js';
import {getHtml} from './tab_organization_results.html.js';
import type {TabOrganization, TabOrganizationSession} from './tab_search.mojom-webui.js';

const MINIMUM_SCROLLABLE_MAX_HEIGHT: number = 204;
const NON_SCROLLABLE_VERTICAL_SPACING: number = 120;

export interface TabOrganizationResultsElement {
  $: {
    feedbackButtons: CrFeedbackButtonsElement,
    header: HTMLElement,
    learnMore: HTMLElement,
    scrollable: HTMLElement,
    selector: IronSelectorElement,
  };
}

export class TabOrganizationResultsElement extends CrLitElement {
  static get is() {
    return 'tab-organization-results';
  }

  static override get properties() {
    return {
      session: {type: Object},
      availableHeight: {type: Number},

      multiTabOrganization: {
        type: Boolean,
        reflect: true,
      },

      feedbackSelectedOption_: {type: Number},
    };
  }

  session?: TabOrganizationSession;
  availableHeight: number = 0;
  multiTabOrganization: boolean = false;

  protected feedbackSelectedOption_: CrFeedbackOption =
      CrFeedbackOption.UNSPECIFIED;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('session')) {
      this.feedbackSelectedOption_ = CrFeedbackOption.UNSPECIFIED;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('session')) {
      this.updateScroll_();
    }

    if (changedProperties.has('availableHeight')) {
      this.onAvailableHeightChange_();
    }
  }

  override firstUpdated() {
    this.$.scrollable.addEventListener('scroll', this.updateScroll_.bind(this));
  }

  focusInput() {
    const group = this.shadowRoot!.querySelector('tab-organization-group');
    if (!group) {
      return;
    }
    group.focusInput();
  }

  private updateScroll_() {
    const scrollable = this.$.scrollable;
    scrollable.classList.toggle(
        'can-scroll', scrollable.clientHeight < scrollable.scrollHeight);
    scrollable.classList.toggle('is-scrolled', scrollable.scrollTop > 0);
    scrollable.classList.toggle(
        'scrolled-to-bottom',
        scrollable.scrollTop + scrollable.clientHeight >=
            scrollable.scrollHeight);
  }

  protected getErrorTitle_(): string {
    if (!this.session) {
      return '';
    }

    const id = this.session.activeTabId;
    if (id === -1) {
      return '';
    }
    let foundTab = false;
    this.getOrganizations_().forEach(organization => {
      organization.tabs.forEach((tab) => {
        if (tab.tabId === id) {
          foundTab = true;
        }
      });
    });
    if (foundTab) {
      return '';
    }
    return loadTimeData.getString('successMissingActiveTabTitle');
  }

  protected getTitle_(): string {
    if (this.multiTabOrganization) {
      if (this.hasMultipleOrganizations_()) {
        return loadTimeData.getStringF(
            'successTitleMulti', this.getOrganizations_().length);
      }
      return loadTimeData.getString('successTitleSingle');
    }
    return loadTimeData.getString('successTitle');
  }

  protected getOrganizations_(): TabOrganization[] {
    if (!this.session) {
      return [];
    }
    if (this.multiTabOrganization) {
      return this.session.organizations;
    } else {
      return this.session.organizations.slice(0, 1);
    }
  }

  protected hasMultipleOrganizations_(): boolean {
    return this.getOrganizations_().length > 1;
  }

  protected getName_(organization: TabOrganization): string {
    return mojoString16ToString(organization.name);
  }

  private onAvailableHeightChange_() {
    const maxHeight = Math.max(
        MINIMUM_SCROLLABLE_MAX_HEIGHT,
        (this.availableHeight - NON_SCROLLABLE_VERTICAL_SPACING));
    this.$.scrollable.style.maxHeight = maxHeight + 'px';
    this.updateScroll_();
  }

  protected onCreateAllGroupsClick_(event: CustomEvent) {
    event.stopPropagation();
    event.preventDefault();

    const groups =
        [...this.shadowRoot!.querySelectorAll('tab-organization-group')];
    const organizations = groups.map((group: TabOrganizationGroupElement) => {
      return {
        organizationId: group.organizationId,
        name: group.name,
        tabs: group.tabs,
      };
    });

    this.fire('create-all-groups-click', {organizations});
  }

  protected onLearnMoreClick_() {
    this.fire('learn-more-click');
  }

  protected onLearnMoreKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      this.onLearnMoreClick_();
    }
  }

  protected onFeedbackKeyDown_(event: KeyboardEvent) {
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

  protected onFeedbackSelectedOptionChanged_(
      event: CustomEvent<{value: CrFeedbackOption}>) {
    this.feedbackSelectedOption_ = event.detail.value;
    this.fire('feedback', {value: this.feedbackSelectedOption_});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-organization-results': TabOrganizationResultsElement;
  }
}

customElements.define(
    TabOrganizationResultsElement.is, TabOrganizationResultsElement);
