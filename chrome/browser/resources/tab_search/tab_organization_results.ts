// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';
import './strings.m.js';
import './tab_organization_group.js';
import './tab_organization_results_actions.js';
import './tab_organization_shared_style.css.js';

import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import type {CrFeedbackButtonsElement} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import type {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {TabOrganizationGroupElement} from './tab_organization_group.js';
import {getTemplate} from './tab_organization_results.html.js';
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

export class TabOrganizationResultsElement extends PolymerElement {
  static get is() {
    return 'tab-organization-results';
  }

  static get properties() {
    return {
      session: {
        type: Object,
        observer: 'onSessionChange_',
      },

      availableHeight: {
        type: Number,
        observer: 'onAvailableHeightChange_',
        value: 0,
      },

      multiTabOrganization: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      feedbackSelectedOption_: {
        type: String,
        value: CrFeedbackOption.UNSPECIFIED,
      },
    };
  }

  session: TabOrganizationSession;
  availableHeight: number;
  multiTabOrganization: boolean;

  private feedbackSelectedOption_: CrFeedbackOption;

  static get template() {
    return getTemplate();
  }

  focusInput() {
    const group = this.shadowRoot!.querySelector('tab-organization-group');
    if (!group) {
      return;
    }
    group.focusInput();
  }

  private getTitle_(): string {
    if (this.multiTabOrganization) {
      if (this.hasMultipleOrganizations_()) {
        return loadTimeData.getStringF(
            'successTitleMulti', this.getOrganizations_().length);
      }
      return loadTimeData.getString('successTitleSingle');
    }
    return loadTimeData.getString('successTitle');
  }

  private getOrganizations_(): TabOrganization[] {
    if (!this.session) {
      return [];
    }
    if (this.multiTabOrganization) {
      return this.session.organizations;
    } else {
      return this.session.organizations.slice(0, 1);
    }
  }

  private hasMultipleOrganizations_() {
    return this.getOrganizations_().length > 1;
  }

  private getName_(organization: TabOrganization) {
    return mojoString16ToString(organization.name);
  }

  private onAvailableHeightChange_() {
    const maxHeight = Math.max(
        MINIMUM_SCROLLABLE_MAX_HEIGHT,
        (this.availableHeight - NON_SCROLLABLE_VERTICAL_SPACING));
    this.$.scrollable.style.maxHeight = maxHeight + 'px';
  }

  private onSessionChange_() {
    this.feedbackSelectedOption_ = CrFeedbackOption.UNSPECIFIED;
  }

  private onCreateAllGroupsClick_(event: CustomEvent) {
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

    this.dispatchEvent(new CustomEvent('create-all-groups-click', {
      bubbles: true,
      composed: true,
      detail: {organizations},
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
    this.feedbackSelectedOption_ = event.detail.value;
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
