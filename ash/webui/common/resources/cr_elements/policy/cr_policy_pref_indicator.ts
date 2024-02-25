// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies that apply to an
 * element controlling a settings preference.
 * Forked from
 * ui/webui/resources/cr_elements/policy/cr_policy_pref_indicator.ts
 */
import '../cr_hidden_style.css.js';
import './cr_tooltip_icon.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyIndicatorMixin, CrPolicyIndicatorType} from './cr_policy_indicator_mixin.js';
import {getTemplate} from './cr_policy_pref_indicator.html.js';
import {CrTooltipIconElement} from './cr_tooltip_icon.js';

const CrPolicyPrefIndicatorElementBase = CrPolicyIndicatorMixin(PolymerElement);

export interface CrPolicyPrefIndicatorElement {
  $: {
    tooltipIcon: CrTooltipIconElement,
  };
}

export class CrPolicyPrefIndicatorElement extends
    CrPolicyPrefIndicatorElementBase {
  static get is() {
    return 'cr-policy-pref-indicator';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      iconAriaLabel: String,

      indicatorType: {
        type: String,
        value: CrPolicyIndicatorType.NONE,
        computed: 'getIndicatorTypeForPref_(pref.*, associatedValue)',
      },

      indicatorTooltip: {
        type: String,
        computed: 'getIndicatorTooltipForPref_(indicatorType, pref.*)',
      },

      /**
       * Optional preference object associated with the indicator. Initialized
       * to null so that computed functions will get called if this is never
       * set.
       */
      pref: Object,

      /**
       * Optional value for the preference value this indicator is associated
       * with. If this is set, no indicator will be shown if it is a member
       * of |pref.userSelectableValues| and is not |pref.recommendedValue|.
       */
      associatedValue: Object,
    };
  }

  iconAriaLabel: string;
  override indicatorType: CrPolicyIndicatorType;
  indicatorTooltip: string;
  pref?: chrome.settingsPrivate.PrefObject;
  associatedValue?: any;

  /**
   * @return The indicator type based on |pref| and |associatedValue|.
   */
  private getIndicatorTypeForPref_(): CrPolicyIndicatorType {
    assert(this.pref);
    const {enforcement, userSelectableValues, controlledBy, recommendedValue} =
        this.pref;
    if (enforcement === chrome.settingsPrivate.Enforcement.RECOMMENDED) {
      if (this.associatedValue !== undefined &&
          this.associatedValue !== recommendedValue) {
        return CrPolicyIndicatorType.NONE;
      }
      return CrPolicyIndicatorType.RECOMMENDED;
    }
    if (enforcement === chrome.settingsPrivate.Enforcement.ENFORCED) {
      // An enforced preference may also have some values still available for
      // the user to select from.
      if (userSelectableValues !== undefined) {
        if (recommendedValue && this.associatedValue === recommendedValue) {
          return CrPolicyIndicatorType.RECOMMENDED;
        } else if (userSelectableValues.includes(this.associatedValue)) {
          return CrPolicyIndicatorType.NONE;
        }
      }
      switch (controlledBy) {
        case chrome.settingsPrivate.ControlledBy.EXTENSION:
          return CrPolicyIndicatorType.EXTENSION;
        case chrome.settingsPrivate.ControlledBy.PRIMARY_USER:
          return CrPolicyIndicatorType.PRIMARY_USER;
        case chrome.settingsPrivate.ControlledBy.OWNER:
          return CrPolicyIndicatorType.OWNER;
        case chrome.settingsPrivate.ControlledBy.USER_POLICY:
          return CrPolicyIndicatorType.USER_POLICY;
        case chrome.settingsPrivate.ControlledBy.DEVICE_POLICY:
          return CrPolicyIndicatorType.DEVICE_POLICY;
        case chrome.settingsPrivate.ControlledBy.PARENT:
          return CrPolicyIndicatorType.PARENT;
        case chrome.settingsPrivate.ControlledBy.CHILD_RESTRICTION:
          return CrPolicyIndicatorType.CHILD_RESTRICTION;
      }
    }
    if (enforcement === chrome.settingsPrivate.Enforcement.PARENT_SUPERVISED) {
      return CrPolicyIndicatorType.PARENT;
    }
    return CrPolicyIndicatorType.NONE;
  }

  /**
   * @return The tooltip text for |indicatorType|.
   */
  private getIndicatorTooltipForPref_(): string {
    if (!this.pref) {
      return '';
    }

    const matches = this.pref && this.pref.value === this.pref.recommendedValue;
    return this.getIndicatorTooltip(
        this.indicatorType, this.pref.controlledByName || '', matches);
  }

  getFocusableElement(): HTMLElement {
    return this.$.tooltipIcon.getFocusableElement();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-policy-pref-indicator': CrPolicyPrefIndicatorElement;
  }
}

customElements.define(
    CrPolicyPrefIndicatorElement.is, CrPolicyPrefIndicatorElement);
