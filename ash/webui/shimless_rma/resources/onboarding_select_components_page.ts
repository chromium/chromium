// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './repair_component_chip.js';
import './shimless_rma_shared.css.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComponentTypeToId} from './data.js';
import {CLICK_REPAIR_COMPONENT_BUTTON, ClickRepairComponentButtonEvent} from './events.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './onboarding_select_components_page.html.js';
import {RepairComponentChip} from './repair_component_chip.js';
import {Component, ComponentRepairStatus, ComponentType, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {enableNextButton, executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

interface ComponentCheckbox {
  component: ComponentType;
  uniqueId: number;
  id: string;
  identifier: string;
  name: string;
  checked: boolean;
  disabled: boolean;
  isFirstClickableComponent?: boolean;
}

declare global {
  interface HTMLElementEventMap {
    [CLICK_REPAIR_COMPONENT_BUTTON]: ClickRepairComponentButtonEvent;
  }
}

/**
 * @fileoverview
 * 'onboarding-select-components-page' is the page for selecting the components
 * that were replaced during repair.
 */

const NUM_COLUMNS = 2;

const OnboardingSelectComponentsPageElementBase = I18nMixin(PolymerElement);

export class OnboardingSelectComponentsPageElement extends
    OnboardingSelectComponentsPageElementBase {
  static get is() {
    return 'onboarding-select-components-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.ts.
       */
      allButtonsDisabled: Boolean,

      componentCheckboxes: {
        type: Array,
        value: () => [],
      },

      reworkFlowLinkText: {type: String, value: ''},

      /**
       * The index into componentCheckboxes for keyboard navigation between
       * components.
       */
      focusedComponentIndex: {
        type: Number,
        value: -1,
      },
    };
  }

  allButtonsDisabled: boolean;
  shimlessRmaService: ShimlessRmaServiceInterface = getShimlessRmaService();
  protected componentCheckboxes: ComponentCheckbox[];
  private reworkFlowLinkText: TrustedHTML;
  private focusedComponentIndex: number;
  private onComponentClickedListener: EventListenerOrEventListenerObject|null =
      null;

  static get observers() {
    return ['updateIsFirstClickableComponent(componentCheckboxes.*)'];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.focusOnCurrentComponent();
    window.addEventListener('keydown', this.handleKeyDownEvent);
    this.onComponentClickedListener = (e) =>
        this.componentClicked((e as ClickRepairComponentButtonEvent));
    window.addEventListener(
        CLICK_REPAIR_COMPONENT_BUTTON, this.onComponentClickedListener);
  }

  /**
   * The componentClickedCallback callback is used to capture events when
   * components are clicked, so that the page can put the focus on the
   * component that was clicked.
   */
  private componentClicked(event: ClickRepairComponentButtonEvent): void {
    const componentIndex = this.componentCheckboxes.findIndex(
        component => component.uniqueId === event.detail);

    if (componentIndex === -1 ||
        this.componentCheckboxes[componentIndex].disabled) {
      return;
    }

    this.focusedComponentIndex = componentIndex;
    this.focusOnCurrentComponent();
  }

  /**
   * Handles keyboard navigation over the list of components.
   */
  private handleKeyDownEvent(event: KeyboardEvent): void {
    if (event.key !== 'ArrowRight' && event.key !== 'ArrowDown' &&
        event.key !== 'ArrowLeft' && event.key !== 'ArrowUp') {
      return;
    }

    // If there are no selectable components, do nothing.
    if (this.focusedComponentIndex === -1) {
      return;
    }

    // Don't use keyboard navigation if the user tabbed out of the
    // component list.
    if (!this.shadowRoot!.activeElement ||
        this.shadowRoot!.activeElement.tagName !== 'REPAIR-COMPONENT-CHIP') {
      return;
    }

    if (event.key === 'ArrowRight' || event.key === 'ArrowDown') {
      // The Down button should send you down the column, so we go forward
      // by two components, which is the size of the row.
      let step = 1;
      if (event.key === 'ArrowDown') {
        step = NUM_COLUMNS;
      }

      let newIndex = this.focusedComponentIndex + step;
      // Keep skipping disabled components until we encounter one that is
      // not disabled.
      while (newIndex < this.componentCheckboxes.length &&
             this.componentCheckboxes[newIndex].disabled) {
        newIndex += step;
      }
      // Check that we haven't ended up outside of the array before
      // applying the changes.
      if (newIndex < this.componentCheckboxes.length) {
        this.focusedComponentIndex = newIndex;
      }
    }

    // The left and up arrows work similarly to down and right, but go
    // backwards.
    if (event.key === 'ArrowLeft' || event.key === 'ArrowUp') {
      let step = 1;
      if (event.key === 'ArrowUp') {
        step = NUM_COLUMNS;
      }

      let newIndex = this.focusedComponentIndex - step;
      while (newIndex >= 0 && this.componentCheckboxes[newIndex].disabled) {
        newIndex -= step;
      }
      if (newIndex >= 0) {
        this.focusedComponentIndex = newIndex;
      }
    }

    this.focusOnCurrentComponent();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('keydown', this.handleKeyDownEvent);
    if (this.onComponentClickedListener) {
      window.removeEventListener(
          CLICK_REPAIR_COMPONENT_BUTTON, this.onComponentClickedListener);
    }
  }

  override ready() {
    super.ready();
    this.setReworkFlowLink();
    this.getComponents();
    enableNextButton(this);

    // Hide the gradient when the list is scrolled to the end.
    const scrollContainer = this.shadowRoot!.querySelector('.scroll-container');
    assert(scrollContainer);
    scrollContainer.addEventListener('scroll', (event: Event) => {
      assert(event && event.target);
      const target = event.target as HTMLElement;
      const gradient: HTMLDivElement|null =
          this.shadowRoot!.querySelector('.gradient');
      assert(gradient);
      if (target.scrollHeight - target.scrollTop === target.clientHeight) {
        gradient.style.setProperty('visibility', 'hidden');
      } else {
        gradient.style.setProperty('visibility', 'visible');
      }
    });

    focusPageTitle(this);
  }

  private async getComponents(): Promise<void> {
    const result = await this.shimlessRmaService.getComponentList();
    if (!result || !result.hasOwnProperty('components')) {
      console.error('Could not get components!');
      return;
    }

    this.componentCheckboxes =
        result.components.map((item: Component, index: number) => {
          assert(item.component);
          return {
            component: item.component,
            uniqueId: index,
            id: ComponentTypeToId[item.component],
            identifier: item.identifier,
            name: this.i18n(ComponentTypeToId[item.component]),
            checked: item.state === ComponentRepairStatus.kReplaced,
            disabled: item.state === ComponentRepairStatus.kMissing,
          };
        });

    // Focus on the first clickable component at the beginning.
    this.focusedComponentIndex =
        this.componentCheckboxes.findIndex(component => !component.disabled);
  }

  /**
   * Make the page focus on the component at focusedComponentIndex.
   */
  private focusOnCurrentComponent(): void {
    if (this.focusedComponentIndex !== -1) {
      const componentChip: RepairComponentChip|null =
          this.shadowRoot!.querySelector(`[unique-id="${
              this.componentCheckboxes[this.focusedComponentIndex]
                  .uniqueId}"]`);
      assert(componentChip);
      const componentButton: CrButtonElement|null =
          componentChip.shadowRoot!.querySelector('#componentButton');
      assert(componentButton);
      componentButton.focus();
    }
  }

  private getComponentRepairStateList(): Component[] {
    return this.componentCheckboxes.map((item: ComponentCheckbox) => {
      let state = ComponentRepairStatus.kOriginal;
      if (item.disabled) {
        state = ComponentRepairStatus.kMissing;
      } else if (item.checked) {
        state = ComponentRepairStatus.kReplaced;
      }
      return {
        component: item.component,
        state: state,
        identifier: item.identifier,
      };
    });
  }

  protected onReworkFlowLinkClicked(e: Event): void {
    e.preventDefault();
    executeThenTransitionState(
        this, () => this.shimlessRmaService.reworkMainboard());
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    return this.shimlessRmaService.setComponentList(
        this.getComponentRepairStateList());
  }

  protected setReworkFlowLink(): void {
    this.reworkFlowLinkText =
        this.i18nAdvanced('reworkFlowLinkText', {attrs: ['id']});
    const linkElement: HTMLAnchorElement|null =
        this.shadowRoot!.querySelector('#reworkFlowLink');
    assert(linkElement);
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', (e: Event) => {
      if (this.allButtonsDisabled) {
        return;
      }

      this.onReworkFlowLinkClicked(e);
    });
  }

  protected isComponentDisabled(componentDisabled: boolean): boolean {
    return this.allButtonsDisabled || componentDisabled;
  }

  private updateIsFirstClickableComponent(): void {
    const firstClickableComponent = this.componentCheckboxes.find(
        (component: ComponentCheckbox) => !component.disabled);
    this.componentCheckboxes.forEach(component => {
      component.isFirstClickableComponent =
          component === firstClickableComponent;
    });
  }

  getComponentRepairStateListForTesting(): Component[] {
    return this.getComponentRepairStateList();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OnboardingSelectComponentsPageElement.is]:
        OnboardingSelectComponentsPageElement;
  }
}

customElements.define(
    OnboardingSelectComponentsPageElement.is,
    OnboardingSelectComponentsPageElement);
