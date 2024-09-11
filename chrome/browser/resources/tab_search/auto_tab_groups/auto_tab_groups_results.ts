// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import './auto_tab_groups_group.js';
import './auto_tab_groups_results_actions.js';

import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import type {CrFeedbackButtonsElement} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {AutoTabGroupsGroupElement} from './auto_tab_groups_group.js';
import {getCss} from './auto_tab_groups_results.css.js';
import {getHtml} from './auto_tab_groups_results.html.js';
import type {TabOrganization, TabOrganizationSession} from '../tab_search.mojom-webui.js';

const MINIMUM_SCROLLABLE_MAX_HEIGHT: number = 204;
const NON_SCROLLABLE_VERTICAL_SPACING: number = 212;

export interface AutoTabGroupsResultsElement {
  $: {
    feedbackButtons: CrFeedbackButtonsElement,
    header: HTMLElement,
    learnMore: HTMLElement,
    scrollable: HTMLElement,
  };
}

export class AutoTabGroupsResultsElement extends CrLitElement {
  static get is() {
    return 'auto-tab-groups-results';
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
      const changedSession = changedProperties.get('session');
      if (changedSession &&
          (!this.session ||
           changedSession.sessionId !== this.session.sessionId)) {
        this.feedbackSelectedOption_ = CrFeedbackOption.UNSPECIFIED;
      }
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('availableHeight')) {
      this.onAvailableHeightChange_();
    }

    if (changedProperties.has('session') ||
        changedProperties.has('multiTabOrganization')) {
      this.updateScroll_();
    }
  }

  override firstUpdated() {
    this.$.scrollable.addEventListener('scroll', this.updateScroll_.bind(this));
  }

  focusInput() {
    const group = this.shadowRoot!.querySelector('auto-tab-groups-group');
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
        scrollable.scrollTop + this.getMaxScrollableHeight_() >=
            scrollable.scrollHeight);
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

  private getMaxScrollableHeight_(): number {
    return Math.max(
        MINIMUM_SCROLLABLE_MAX_HEIGHT,
        (this.availableHeight - NON_SCROLLABLE_VERTICAL_SPACING));
  }

  private onAvailableHeightChange_() {
    const maxHeight = this.getMaxScrollableHeight_();
    this.$.scrollable.style.maxHeight = maxHeight + 'px';
    this.updateScroll_();
  }

  protected onCreateAllGroupsClick_(event: CustomEvent) {
    event.stopPropagation();
    event.preventDefault();

    const groups =
        [...this.shadowRoot!.querySelectorAll('auto-tab-groups-group')];
    const organizations = groups.map((group: AutoTabGroupsGroupElement) => {
      return {
        organizationId: group.organizationId,
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
    'auto-tab-groups-results': AutoTabGroupsResultsElement;
  }
}

customElements.define(
    AutoTabGroupsResultsElement.is, AutoTabGroupsResultsElement);
