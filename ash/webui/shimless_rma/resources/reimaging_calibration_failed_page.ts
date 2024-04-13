// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './base_page.js';
import './calibration_component_chip.js';
import './icons.html.js';
import './shimless_rma_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CalibrationComponentChipElement} from './calibration_component_chip.js';
import {ComponentTypeToId} from './data.js';
import {CLICK_CALIBRATION_COMPONENT_BUTTON, ClickCalibrationComponentEvent} from './events.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {getTemplate} from './reimaging_calibration_failed_page.html.js';
import {CalibrationComponentStatus, CalibrationStatus, ComponentType, ShimlessRmaServiceInterface, StateResult} from './shimless_rma.mojom-webui.js';
import {disableNextButton, enableNextButton, executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'reimaging-calibration-failed-page' is to inform the user which components
 * will be calibrated and allow them to skip components if necessary.
 * (Skipping components could allow the device to be in a usable, but not fully
 * functioning state.)
 */

interface ComponentCheckbox {
  component: ComponentType;
  uniqueId: number;
  id: string;
  name: string;
  checked: boolean;
  failed: boolean;
  disabled?: boolean;
  isFirstClickableComponent?: boolean;
}

declare global {
  interface WindowEventMap {
    [CLICK_CALIBRATION_COMPONENT_BUTTON]: ClickCalibrationComponentEvent;
  }
}


const NUM_COLUMNS = 1;

const ReimagingCalibrationFailedPageBase = I18nMixin(PolymerElement);

export class ReimagingCalibrationFailedPage extends
    ReimagingCalibrationFailedPageBase {
  static get is() {
    return 'reimaging-calibration-failed-page' as const;
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
  private componentCheckboxes: ComponentCheckbox[];
  private focusedComponentIndex: number;
  componentClicked: (event: ClickCalibrationComponentEvent) => void;
  handleKeyDownEvent: (event: KeyboardEvent) => void;
  onExitButtonClick: () => Promise<{stateResult: StateResult}>;

  static get observers() {
    return [
      'updateIsFirstClickableComponent(componentCheckboxes.*)',
      'updateNextButtonAvailability(componentCheckboxes.*)',
    ];
  }

  constructor() {
    super();
    /**
     * The componentClickedCallback callback is used to capture events when
     * components are clicked, so that the page can put the focus on the
     * component that was clicked.
     */
    this.componentClicked = (event) => {
      const componentIndex = this.componentCheckboxes.findIndex(
          component => component.uniqueId === event.detail);

      if (componentIndex === -1 ||
          this.componentCheckboxes[componentIndex].disabled) {
        return;
      }

      this.focusedComponentIndex = componentIndex;
      this.focusOnCurrentComponent();
    };

    /**
     * Handles keyboard navigation over the list of components.
     * TODO(240717594): Find a way to avoid duplication of this code in the
     * repair components page.
     */
    this.handleKeyDownEvent = (event: KeyboardEvent) => {
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
          this.shadowRoot!.activeElement.tagName !==
              'CALIBRATION-COMPONENT-CHIP') {
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
    };

    /**
     * The "Skip calibration" button on this page is styled and positioned like
     * a exit button. So we use the common exit button from shimless_rma.ts
     * This function needs to be public, because it's invoked by
     * shimless_rma.ts as part of the response to the exit button click.
     */
    this.onExitButtonClick = () => {
      if (this.tryingToSkipWithFailedComponents()) {
        const dialog: CrDialogElement|null =
            this.shadowRoot!.querySelector('#failedComponentsDialog');
        assert(dialog);
        dialog.showModal();
        return Promise.reject(
            new Error('Attempting to skip with failed components.'));
      }

      return this.skipCalibration();
    };
  }

  override ready() {
    super.ready();
    this.getInitialComponentsList();

    // Hide the gradient when the list is scrolled to the end.
    this.shadowRoot!.querySelector('.scroll-container')!.addEventListener(
        'scroll', (event: Event) => {
          const gradient =
              this.shadowRoot!.querySelector<HTMLElement>('.gradient');
          assert(gradient);
          const dialog = this.shadowRoot!.querySelector<CrDialogElement>(
              '#failedComponentsDialog');
          assert(dialog);
          dialog.close();
          const target = (event.target as HTMLElement);
          assert(target);
          if (target.scrollHeight - target.scrollTop === target.clientHeight) {
            gradient.style.setProperty('visibility', 'hidden');
          } else {
            gradient.style.setProperty('visibility', 'visible');
          }
        });

    focusPageTitle(this);
  }

  private getInitialComponentsList(): void {
    this.shimlessRmaService.getCalibrationComponentList().then((result) => {
      if (!result || !result.hasOwnProperty('components')) {
        // TODO(gavindodd): Set an error state?
        console.error('Could not get components!');
        return;
      }

      this.componentCheckboxes = result.components.map((item, index) => {
        return {
          component: item.component,
          uniqueId: index,
          id: ComponentTypeToId[item.component],
          name: this.i18n(ComponentTypeToId[item.component]),
          checked: false,
          failed: item.status === CalibrationStatus.kCalibrationFailed,
          // Disable components that did not fail calibration so they can't be
          // selected for calibration again.
          disabled: item.status !== CalibrationStatus.kCalibrationFailed,
        };
      });

      // Focus on the first clickable component at the beginning.
      this.focusedComponentIndex =
          this.componentCheckboxes.findIndex(component => !component.disabled);
    });
  }

  override connectedCallback() {
    super.connectedCallback();
    window.addEventListener('keydown', this.handleKeyDownEvent);
    window.addEventListener(
        CLICK_CALIBRATION_COMPONENT_BUTTON, this.componentClicked);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('keydown', this.handleKeyDownEvent);
    window.removeEventListener(
        CLICK_CALIBRATION_COMPONENT_BUTTON, this.componentClicked);
  }

  private focusOnCurrentComponent() {
    if (this.focusedComponentIndex !== -1) {
      const componentChip: CalibrationComponentChipElement|null =
          this.shadowRoot!.querySelector(`[unique-id="${
              this.componentCheckboxes[this.focusedComponentIndex]
                  .uniqueId}"]`);
      assert(componentChip);
      const button = componentChip.shadowRoot!.querySelector<HTMLElement>(
          '#componentButton');
      assert(button);
      button.focus();
    }
  }

  private getComponentsList(): CalibrationComponentStatus[] {
    return this.componentCheckboxes.map(item => {
      // These statuses tell rmad how to treat each component in this request.
      // If the component didn't fail a calibration, its status needs to be
      // `kCalibrationComplete`. If the user checked a component, it wants to
      // retry its calibration. If the user didn't select a failed component for
      // retry, then skip it.
      let status;
      if (!item.failed) {
        status = CalibrationStatus.kCalibrationComplete;
      } else if (item.checked) {
        status = CalibrationStatus.kCalibrationWaiting;
      } else {
        status = CalibrationStatus.kCalibrationSkip;
      }

      return {
        component: item.component,
        status: status,
        progress: 0.0,
      };
    });
  }

  private skipCalibration(): Promise<{stateResult: StateResult}> {
    const skippedComponents = this.componentCheckboxes.map(item => {
      return {
        component: item.component,
        // This status tells rmad how to treat each component in this request.
        // Because the user requested to skip all calibrations, make sure to
        // only mark the failed components as `kCalibrationSkip`.
        status: item.failed ? CalibrationStatus.kCalibrationSkip :
                              CalibrationStatus.kCalibrationComplete,
        progress: 0.0,
      };
    });
    return this.shimlessRmaService.startCalibration(skippedComponents);
  }

  onNextButtonClick(): Promise<{stateResult: StateResult}> {
    return this.shimlessRmaService.startCalibration(this.getComponentsList());
  }

  private isComponentDisabled(componentDisabled: boolean): boolean {
    return componentDisabled || this.allButtonsDisabled;
  }

  protected onSkipDialogButtonClicked(): void {
    this.closeDialog();
    executeThenTransitionState(this, () => this.skipCalibration());
  }

  protected closeDialog(): void {
    const dialog: CrDialogElement|null =
        this.shadowRoot!.querySelector('#failedComponentsDialog');
    assert(dialog);
    dialog.close();
  }

  private tryingToSkipWithFailedComponents(): boolean {
    return this.componentCheckboxes.some(
        component => component.failed && !component.checked);
  }

  private updateIsFirstClickableComponent(): void {
    const firstClickableComponent =
        this.componentCheckboxes.find(component => !component.disabled);
    this.componentCheckboxes.forEach(component => {
      component.isFirstClickableComponent =
          (component === firstClickableComponent) ? true : false;
    });
  }

  private updateNextButtonAvailability(): void {
    if (this.componentCheckboxes.some(component => component.checked)) {
      enableNextButton(this);
    } else {
      disableNextButton(this);
    }
  }

  getComponentsListForTesting(): CalibrationComponentStatus[] {
    return this.getComponentsList();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ReimagingCalibrationFailedPage.is]: ReimagingCalibrationFailedPage;
  }
}

customElements.define(
    ReimagingCalibrationFailedPage.is, ReimagingCalibrationFailedPage);
