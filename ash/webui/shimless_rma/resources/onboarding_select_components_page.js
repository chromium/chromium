// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './repair_component_chip.js';
import './shimless_rma_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ComponentTypeToId} from './data.js';
import {getShimlessRmaService} from './mojo_interface_provider.js';
import {Component, ComponentRepairStatus, ComponentType, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {enableNextButton, executeThenTransitionState, focusPageTitle} from './shimless_rma_util.js';

/**
 * @typedef {{
 *   component: !ComponentType,
 *   uniqueId: number,
 *   id: string,
 *   identifier: string,
 *   name: string,
 *   checked: boolean,
 *   disabled: boolean
 * }}
 */
let ComponentCheckbox;

/**
 * @fileoverview
 * 'onboarding-select-components-page' is the page for selecting the components
 * that were replaced during repair.
 */

const NUM_COLUMNS = 2;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingSelectComponentsPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingSelectComponentsPageElement extends
    OnboardingSelectComponentsPageElementBase {
  static get is() {
    return 'onboarding-select-components-page';
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

      /** @protected {!Array<!ComponentCheckbox>} */
      componentCheckboxes_: {
        type: Array,
        value: () => [],
      },

      /** @private {string} */
      reworkFlowLinkText_: {type: String, value: ''},

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
    return ['updateIsFirstClickableComponent_(componentCheckboxes_.*)'];
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
     * @private {?Function}
     */
    this.handleKeyDownEvent = (event) => {
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
          this.shadowRoot.activeElement.tagName !== 'REPAIR-COMPONENT-CHIP') {
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
  }

  /** @override */
  ready() {
    super.ready();
    this.setReworkFlowLink_();
    this.getComponents_();
    enableNextButton(this);

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
  getComponents_() {
    this.shimlessRmaService_.getComponentList().then((result) => {
      if (!result || !result.hasOwnProperty('components')) {
        // TODO(gavindodd): Set an error state?
        console.error('Could not get components!');
        return;
      }

      this.componentCheckboxes_ = result.components.map((item, index) => {
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
      this.focusedComponentIndex_ =
          this.componentCheckboxes_.findIndex(component => !component.disabled);
    });
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('keydown', this.handleKeyDownEvent);
    window.addEventListener(
        'click-repair-component-button', this.componentClicked_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('keydown', this.handleKeyDownEvent);
    window.removeEventListener(
        'click-repair-component-button', this.componentClicked_);
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
   * @return {!Array<!Component>}
   * @private
   */
  getComponentRepairStateList_() {
    return this.componentCheckboxes_.map(item => {
      /** @type {!ComponentRepairStatus} */
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

  /** @protected */
  onReworkFlowLinkClicked_(e) {
    e.preventDefault();
    executeThenTransitionState(
        this, () => this.shimlessRmaService_.reworkMainboard());
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.setComponentList(
        this.getComponentRepairStateList_());
  }

  /** @protected */
  setReworkFlowLink_() {
    this.reworkFlowLinkText_ =
        this.i18nAdvanced('reworkFlowLinkText', {attrs: ['id']});
    const linkElement = this.shadowRoot.querySelector('#reworkFlowLink');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', e => {
      if (this.allButtonsDisabled) {
        return;
      }

      this.onReworkFlowLinkClicked_(e);
    });
  }

  /**
   * @param {boolean} componentDisabled
   * @return {boolean}
   * @protected
   */
  isComponentDisabled_(componentDisabled) {
    return this.allButtonsDisabled || componentDisabled;
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
}

customElements.define(
    OnboardingSelectComponentsPageElement.is,
    OnboardingSelectComponentsPageElement);
