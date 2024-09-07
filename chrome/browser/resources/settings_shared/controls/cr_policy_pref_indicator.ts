// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies that apply to an
 * element controlling a settings preference.
 */
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.js';

import {CrPolicyIndicatorType} from '//resources/cr_elements/policy/cr_policy_types.js';
import type {CrTooltipIconElement} from '//resources/cr_elements/policy/cr_tooltip_icon.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_policy_pref_indicator.html.js';

export interface CrPolicyPrefIndicatorElement {
  $: {
    tooltipIcon: CrTooltipIconElement,
  };
}

export class CrPolicyPrefIndicatorElement extends PolymerElement {
  static get is() {
    return 'cr-policy-pref-indicator';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      iconAriaLabel: String,

      indicatorIcon: {
        type: String,
        computed: 'getIndicatorIcon_(indicatorType)',
      },

      indicatorType: {
        type: String,
        value: CrPolicyIndicatorType.NONE,
        computed: 'getIndicatorTypeForPref_(pref.*, associatedValue)',
      },

      indicatorTooltip: {
        type: String,
        computed: 'getIndicatorTooltipForPref_(indicatorType, pref.*)',
      },

      indicatorVisible: {
        type: Boolean,
        computed: 'getIndicatorVisible_(indicatorType)',
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
  indicatorIcon: string;
  indicatorType: CrPolicyIndicatorType;
  indicatorTooltip: string;
  indicatorVisible: boolean;
  pref?: chrome.settingsPrivate.PrefObject;
  associatedValue?: any;

  /**
   * @return True if the indicator should be shown.
   */
  private getIndicatorVisible_(type: CrPolicyIndicatorType): boolean {
    return type !== CrPolicyIndicatorType.NONE;
  }

  /**
   * @return {string} The cr-icon icon name.
   */
  private getIndicatorIcon_(type: CrPolicyIndicatorType): string {
    switch (type) {
      case CrPolicyIndicatorType.EXTENSION:
        return 'cr:extension';
      case CrPolicyIndicatorType.NONE:
        return '';
      case CrPolicyIndicatorType.PRIMARY_USER:
        return 'cr:group';
      case CrPolicyIndicatorType.OWNER:
        return 'cr:person';
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
      case CrPolicyIndicatorType.RECOMMENDED:
        return 'cr20:domain';
      case CrPolicyIndicatorType.PARENT:
      case CrPolicyIndicatorType.CHILD_RESTRICTION:
        return 'cr20:kite';
      default:
        assertNotReached();
    }
  }

  /**
   * @param name The name associated with the indicator. See
   *     chrome.settingsPrivate.PrefObject.controlledByName
   * @param matches For RECOMMENDED only, whether the indicator
   *     value matches the recommended value.
   * @return The tooltip text for |type|.
   */
  private getIndicatorTooltip_(
      type: CrPolicyIndicatorType, name: string, matches?: boolean): string {
    if (!window.CrPolicyStrings) {
      return '';
    }  // Tooltips may not be defined, e.g. in OOBE.

    const CrPolicyStrings = window.CrPolicyStrings;
    switch (type) {
      case CrPolicyIndicatorType.EXTENSION:
        return name.length > 0 ?
            CrPolicyStrings.controlledSettingExtension!.replace('$1', name) :
            CrPolicyStrings.controlledSettingExtensionWithoutName!;
      // <if expr="chromeos_ash">
      case CrPolicyIndicatorType.PRIMARY_USER:
        return CrPolicyStrings.controlledSettingShared!.replace('$1', name);
      case CrPolicyIndicatorType.OWNER:
        return name.length > 0 ?
            CrPolicyStrings.controlledSettingWithOwner!.replace('$1', name) :
            CrPolicyStrings.controlledSettingNoOwner!;
      // </if>
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
        return CrPolicyStrings.controlledSettingPolicy!;
      case CrPolicyIndicatorType.RECOMMENDED:
        return matches ? CrPolicyStrings.controlledSettingRecommendedMatches! :
                         CrPolicyStrings.controlledSettingRecommendedDiffers!;
      case CrPolicyIndicatorType.PARENT:
        return CrPolicyStrings.controlledSettingParent!;
      case CrPolicyIndicatorType.CHILD_RESTRICTION:
        return CrPolicyStrings.controlledSettingChildRestriction!;
    }
    return '';
  }

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
    return this.getIndicatorTooltip_(
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
