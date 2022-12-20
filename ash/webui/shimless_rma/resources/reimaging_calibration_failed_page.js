// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './base_page.js';
import './calibration_component_chip.js';
import './icons.js';
import './shimless_rma_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComponentTypeToId} from './data.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CalibrationComponentStatus, CalibrationStatus, ComponentType, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableNextButton, enableNextButton, executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'reimaging-calibration-failed-page' is to inform the user which components
 * will be calibrated and allow them to skip components if necessary.
 * (Skipping components could allow the device to be in a usable, but not fully
 * functioning state.)
 */

/**
 * @typedef {{
 *   component: !ComponentType,
 *   uniqueId: number,
 *   id: string,
 *   name: string,
 *   checked: boolean,
 *   failed: boolean,
 * }}
 */
let ComponentCheckbox;

const NUM_COLUMNS = 1;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ReimagingCalibrationFailedPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ReimagingCalibrationFailedPage extends
    ReimagingCalibrationFailedPageBase {
  static get is() {
    return 'reimaging-calibration-failed-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,

      /** @private {!Array<!ComponentCheckbox>} */
      componentCheckboxes_: {
        type: Array,
        value: () => [],
      },

      /**
       * The index into componentCheckboxes_ for keyboard navigation between
       * components.
       * @private
       */
      focusedComponentIndex_: {
        type: Number,
        value: -1,
      },
    };
  }

  static get observers() {
    return [
      'updateIsFirstClickableComponent_(componentCheckboxes_.*)',
      'updateNextButtonAvailability_(componentCheckboxes_.*)',
    ];
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();

    /**
     * The componentClickedCallback_ callback is used to capture events when
     * components are clicked, so that the page can put the focus on the
     * component that was clicked.
     * @private {?Function}
     */
    this.componentClicked_ = (event) => {
      const componentIndex = this.componentCheckboxes_.findIndex(
          component => component.uniqueId === event.detail);

      if (componentIndex === -1 ||
          this.componentCheckboxes_[componentIndex].disabled) {
        return;
      }

      this.focusedComponentIndex_ = componentIndex;
      this.focusOnCurrentComponent_();
    };

    /**
     * Handles keyboard navigation over the list of components.
     * TODO(240717594): Find a way to avoid duplication of this code in the
     * repair components page.
     * @private {?Function}
     */
    this.HandleKeyDownEvent = (event) => {
      if (event.key !== 'ArrowRight' && event.key !== 'ArrowDown' &&
          event.key !== 'ArrowLeft' && event.key !== 'ArrowUp') {
        return;
      }

      // If there are no selectable components, do nothing.
      if (this.focusedComponentIndex_ === -1) {
        return;
      }

      // Don't use keyboard navigation if the user tabbed out of the
      // component list.
      if (!this.shadowRoot.activeElement ||
          this.shadowRoot.activeElement.tagName !==
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

        let newIndex = this.focusedComponentIndex_ + step;
        // Keep skipping disabled components until we encounter one that is
        // not disabled.
        while (newIndex < this.componentCheckboxes_.length &&
               this.componentCheckboxes_[newIndex].disabled) {
          newIndex += step;
        }
        // Check that we haven't ended up outside of the array before
        // applying the changes.
        if (newIndex < this.componentCheckboxes_.length) {
          this.focusedComponentIndex_ = newIndex;
        }
      }

      // The left and up arrows work similarly to down and right, but go
      // backwards.
      if (event.key === 'ArrowLeft' || event.key === 'ArrowUp') {
        let step = 1;
        if (event.key === 'ArrowUp') {
          step = NUM_COLUMNS;
        }

        let newIndex = this.focusedComponentIndex_ - step;
        while (newIndex >= 0 && this.componentCheckboxes_[newIndex].disabled) {
          newIndex -= step;
        }
        if (newIndex >= 0) {
          this.focusedComponentIndex_ = newIndex;
        }
      }

      this.focusOnCurrentComponent_();
    };

    /**
     * The "Skip calibration" button on this page is styled and positioned like
     * a exit button. So we use the common exit button from shimless_rma.js
     * This function needs to be public, because it's invoked by
     * shimless_rma.js as part of the response to the exit button click.
     * @return {!Promise<!{stateResult: !StateResult}>}
     */
    this.onExitButtonClick = () => {
      if (this.tryingToSkipWithFailedComponents_()) {
        this.shadowRoot.querySelector('#failedComponentsDialog').showModal();
        return Promise.reject(
            new Error('Attempting to skip with failed components.'));
      }

      return this.skipCalibration_();
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.getInitialComponentsList_();

    // Hide the gradient when the list is scrolled to the end.
    this.shadowRoot.querySelector('.scroll-container')
        .addEventListener('scroll', (event) => {
          const gradient = this.shadowRoot.querySelector('.gradient');
          if (event.target.scrollHeight - event.target.scrollTop ===
              event.target.clientHeight) {
            gradient.style.setProperty('visibility', 'hidden');
          } else {
            gradient.style.setProperty('visibility', 'visible');
          }
        });

    focusPageTitle(this);
  }

  /** @private */
  getInitialComponentsList_() {
    this.shimlessRmaService_.getCalibrationComponentList().then((result) => {
      if (!result || !result.hasOwnProperty('components')) {
        // TODO(gavindodd): Set an error state?
        console.error('Could not get components!');
        return;
      }

      this.componentCheckboxes_ = result.components.map((item, index) => {
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
      this.focusedComponentIndex_ =
          this.componentCheckboxes_.findIndex(component => !component.disabled);
    });
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('keydown', this.HandleKeyDownEvent);
    window.addEventListener(
        'click-calibration-component-button', this.componentClicked_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('keydown', this.HandleKeyDownEvent);
    window.removeEventListener(
        'click-calibration-component-button', this.componentClicked_);
  }

  /**
   * Make the page focus on the component at focusedComponentIndex_.
   * @private
   */
  focusOnCurrentComponent_() {
    if (this.focusedComponentIndex_ != -1) {
      const componentChip = this.shadowRoot.querySelector(`[unique-id="${
          this.componentCheckboxes_[this.focusedComponentIndex_].uniqueId}"]`);
      componentChip.shadowRoot.querySelector('#componentButton').focus();
    }
  }


  /**
   * @return {!Array<!CalibrationComponentStatus>}
   * @private
   */
  getComponentsList_() {
    return this.componentCheckboxes_.map(item => {
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

  /**
   * @return {!Promise<!{stateResult: !StateResult}>}
   * @private
   */
  skipCalibration_() {
    const skippedComponents = this.componentCheckboxes_.map(item => {
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
    return this.shimlessRmaService_.startCalibration(skippedComponents);
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.startCalibration(this.getComponentsList_());
  }

  /**
   * @param {boolean} componentDisabled
   * @return {boolean}
   * @private
   */
  isComponentDisabled_(componentDisabled) {
    return componentDisabled || this.allButtonsDisabled;
  }

  /** @protected */
  onSkipDialogButtonClicked_() {
    this.closeDialog_();
    executeThenTransitionState(this, () => this.skipCalibration_());
  }

  /** @protected */
  closeDialog_() {
    this.shadowRoot.querySelector('#failedComponentsDialog').close();
  }

  /**
   * @return {boolean}
   * @private
   */
  tryingToSkipWithFailedComponents_() {
    return this.componentCheckboxes_.some(
        component => component.failed && !component.checked);
  }

  /** @private */
  updateIsFirstClickableComponent_() {
    const firstClickableComponent =
        this.componentCheckboxes_.find(component => !component.disabled);
    this.componentCheckboxes_.forEach(component => {
      component.isFirstClickableComponent =
          (component === firstClickableComponent) ? true : false;
    });
  }

  /** @private */
  updateNextButtonAvailability_() {
    if (this.componentCheckboxes_.some(component => component.checked)) {
      enableNextButton(this);
    } else {
      disableNextButton(this);
    }
  }
}

customElements.define(
    ReimagingCalibrationFailedPage.is, ReimagingCalibrationFailedPage);
