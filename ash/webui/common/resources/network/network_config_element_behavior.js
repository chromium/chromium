// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for network config elements.
 */

import {OncMojo} from './onc_mojo.js';

/** @polymerBehavior */
export const NetworkConfigElementBehavior = {
  properties: {
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    readonly: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * Network managed property associated with the config element.
     * @type {?OncMojo.ManagedProperty}
     */
    property: {
      type: Object,
      value: null,
    },

    /**
     * @type {string|number|null}
     */
    value: {
      type: Object,
      notify: true,
    },

    /**
     * If set, the field will be filled and the element will be disabled for
     * user input.
     * @type {string|number|null}
     */
    prefilledValue: {
      type: Object,
      value: null,
    },
  },

  observers:
      ['maybeLockByPrefilledValue(readonly, disabled, value, prefilledValue)'],

  /**
   * @param {boolean} disabled
   * @param {?OncMojo.ManagedProperty} property
   * @return {boolean} True if the element should be disabled.
   * @protected
   */
  getDisabled_(disabled, property) {
    return disabled || (!!property && this.isNetworkPolicyEnforced(property));
  },

  /**
   * It can be overridden by the elements to implement their own validation
   * logic.
   */
  isPrefilledValueValid() {
    return true;
  },

  /**
   * It can be overridden by the elements to implement their extra logic for
   * prefilled value.
   */
  extraSetupForPrefilledValue() {
    return;
  },

  /**
   * If the prefilled value is defined, always use the value and mark the input
   * as readonly.
   */
  maybeLockByPrefilledValue() {
    if (this.prefilledValue === undefined || this.prefilledValue === null) {
      return;
    }
    if (!this.isPrefilledValueValid()) {
      return;
    }
    this.value = this.prefilledValue;
    this.readonly = true;
    this.extraSetupForPrefilledValue();
  },
};
