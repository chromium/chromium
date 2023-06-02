// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {dom, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {invokePolymerMethod} from '../../display_manager.js';
// clang-format on

/**
 * @fileoverview
 * 'MultiStepBehavior' is a behavior that simplifies defining and handling
 * elements that have different UI depending on the state, e.g. screens that
 * have several steps.
 *
 * This behavior controls elements in local DOM marked with for-step attribute.
 * Attribute value should be a comma-separated list of steps for which element
 * should be visible.
 *
 * Example usage (note that element can be shown for multiple steps as well as
 * multiple elements can be show for same step):
 *
 * <dom-module id="my-element">
 *     <template>
 *       ...
 *       <loading-indicator for-step="loading"></loading-indicator>
 *       <some-dialog for-step="action">
 *         ...
 *       </some-dialog>
 *       <error-dialog for-step="error-1,error-2">
 *         <error-message for-step="error-1">...</error-message>
 *         <error-message for-step="error-2">...</error-message>
 *         <illustration for-step="error-2">...</illustration>
 *         ...
 *       </error-dialog>
 *     </template>
 * </dom-module>
 */

/** @polymerBehavior */
export var MultiStepBehavior = {
  properties: {
    uiStep: {
      type: String,
      value: '',
    },
  },

  /*
   * List of UI states, should be replaced by implementing component.
   * Can be a list or enum-style map.
   */
  UI_STEPS: [],

  /*
   * Map from step name to elements that should be visible on that step.
   * Used for performance optimization.
   */
  stepElements_: {},

  /*
   * Whether the element is shown (Between onBeforeShow and onBeforeHide calls).
   */
  shown_: false,

  /*
   * Method that lists all possible steps for current element.
   * Default implementation uses value UI_STEPS that can be either array or
   * enum-style object.
   * Element that delegate part of the states to the child element might
   * need to override this method.
   *
   * @return {Array<string>}
   */
  listSteps() {
    if (Array.isArray(this.UI_STEPS)) {
      return this.UI_STEPS.slice();
    }
    const result = [];
    for (const [key, value] of Object.entries(this.UI_STEPS)) {
      result.push(value);
    }
    return result;
  },

  /*
   * Element should override this method to return name of the initial step.
   * @return {string}
   */
  defaultUIStep() {
    throw new Error('Element should define default UI state');
  },

  ready() {
    // Add marker class for quickly finding children with same behavior.
    this.classList.add('multi-state-element');
    this.refreshStepBindings_();
  },

  onBeforeShow() {
    this.shown_ = true;
    // Only set uiStep to defaultUIStep if it is not set yet.
    if (!this.uiStep) {
      this.setUIStep(this.defaultUIStep());
    } else {
      this.showUIStep_(this.uiStep);
    }
  },

  onBeforeHide() {
    if (this.uiStep) {
      this.hideUIStep_(this.uiStep);
    }
    this.shown_ = false;
  },

  /**
   * Returns default event target element.
   * @type {Object}
   */
  get defaultControl() {
    return this.stepElements_[this.defaultUIStep()][0];
  },

  /*
   * Method that applys a function to all elements of a step (default to
   * current step).
   */
  applyToStepElements(func, step = this.uiStep) {
    for (const element of this.stepElements_[step] || []) {
      func(element);
    }
  },

  setUIStep(step) {
    if (this.uiStep) {
      if (this.uiStep == step) {
        return;
      }
      this.hideUIStep_(this.uiStep);
    }
    this.uiStep = step;
    this.shadowRoot.host.setAttribute('multistep', step);
    this.showUIStep_(this.uiStep);
  },

  showUIStep_(step) {
    if (!this.shown_) {
      // Will execute from onBeforeShow.
      return;
    }
    for (const element of this.stepElements_[step] || []) {
      invokePolymerMethod(element, 'onBeforeShow');
      element.hidden = false;
      // Trigger show() if element is an oobe-dialog
      if (element.show && typeof element.show === 'function') {
        element.show();
      }
    }
  },

  hideUIStep_(step) {
    for (const element of this.stepElements_[step] || []) {
      invokePolymerMethod(element, 'onBeforeHide');
      element.hidden = true;
    }
  },

  /*
   * Fills stepElements_ map by looking up child elements with for-step
   * attribute
   * @private
   */
  refreshStepBindings_() {
    this.stepElements_ = {};
    var matches = dom(this.root).querySelectorAll('[for-step]');
    for (const child of matches) {
      const stepsList = child.getAttribute('for-step');
      for (const stepChunk of stepsList.split(',')) {
        const step = stepChunk.trim();
        const list = this.stepElements_[step] || [];
        list.push(child);
        this.stepElements_[step] = list;
      }
      child.hidden = true;
    }
  },

};

/**
 * TODO(b/24294625): Replace with an interface.
 * @typedef {{
 *   setUIStep: function(string),
 *   onBeforeShow: function(),
 *   onBeforeHide: function(),
 * }}
 */
MultiStepBehavior.Proto;

/** @interface */
export class MultiStepBehaviorInterface {
  setUIStep(step) {}
  /** @return {string} */
  defaultUIStep() {}

  /** @return {string} */
  get uiStep() {}
}
