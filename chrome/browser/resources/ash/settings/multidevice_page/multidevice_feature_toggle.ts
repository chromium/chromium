// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A toggle button specially suited for the MultiDevice Settings UI use-case.
 * Instead of changing on clicks, it requests a pref change from the
 * MultiDevice service and/or triggers a password check to grab an auth token
 * for the user. It also receives real time updates on feature states and
 * reflects them in the toggle status.
 */

import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';

import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiDeviceFeature, MultiDeviceFeatureState} from './multidevice_constants.js';
import {MultiDeviceFeatureMixin} from './multidevice_feature_mixin.js';
import {getTemplate} from './multidevice_feature_toggle.html.js';

export interface SettingsMultideviceFeatureToggleElement {
  $: {
    toggle: CrToggleElement,
  };
}

const SettingsMultideviceFeatureToggleElementBase =
    MultiDeviceFeatureMixin(PolymerElement);

export class SettingsMultideviceFeatureToggleElement extends
    SettingsMultideviceFeatureToggleElementBase {
  static get is() {
    return 'settings-multidevice-feature-toggle' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      feature: Number,
      toggleAriaLabel: String,
      checked_: Boolean,
    };
  }

  static get observers() {
    return ['resetChecked_(feature, pageContentData)'];
  }

  feature: MultiDeviceFeature;
  toggleAriaLabel: string;
  private checked_: boolean;

  override ready(): void {
    super.ready();

    this.addEventListener('click', this.onDisabledInnerToggleClick_);
  }

  override focus(): void {
    this.$.toggle.focus();
  }

  /**
   * Callback for clicking on the toggle itself, or a row containing a toggle
   * without other links. It attempts to toggle the feature's status if the user
   * is allowed.
   */
  toggleFeature(): void {
    this.resetChecked_();

    // Pass the negation of |this.checked_|: this indicates that if the toggle
    // is checked, the intent is for it to be unchecked, and vice versa.
    const featureToggleClickedEvent =
        new CustomEvent('feature-toggle-clicked', {
          bubbles: true,
          composed: true,
          detail: {feature: this.feature, enabled: !this.checked_},
        });
    this.dispatchEvent(featureToggleClickedEvent);
  }

  /**
   * Because MultiDevice prefs are only meant to be controlled via the
   * MultiDevice mojo service, we need the cr-toggle to appear not to change
   * when pressed. This method resets it before a change is visible to the
   * user.
   */
  private resetChecked_(): void {
    // If Phone Hub notification access is prohibited, the toggle is always off.
    if (this.feature === MultiDeviceFeature.PHONE_HUB_NOTIFICATIONS &&
        this.isPhoneHubNotificationAccessProhibited()) {
      this.checked_ = false;
      return;
    }

    // If Phone Hub apps access is prohibited, the toggle is always off.
    if (this.feature === MultiDeviceFeature.ECHE &&
        this.isPhoneHubAppsAccessProhibited()) {
      this.checked_ = false;
      return;
    }

    this.checked_ = this.getFeatureState(this.feature) ===
        MultiDeviceFeatureState.ENABLED_BY_USER;
  }

  /**
   * This handles the edge case in which the inner toggle (i.e., the cr-toggle)
   * is disabled. For context, the cr-toggle element naturally stops clicks
   * from propagating as long as its disabled attribute is false. However, if
   * the cr-toggle's disabled attribute is set to true, its pointer-event CSS
   * property is set to 'none' automatically. Thus, if the cr-toggle is clicked
   * while it is disabled, the click event targets the parent element directly
   * instead of propagating through the cr-toggle. This handler prevents such a
   * click from unintentionally bubbling up the tree.
   */
  private onDisabledInnerToggleClick_(event: Event): void {
    event.stopPropagation();
  }

  /**
   * Returns the A11y label for the toggle.
   */
  private getToggleA11yLabel_(): string {
    return this.toggleAriaLabel || this.getFeatureName(this.feature);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultideviceFeatureToggleElement.is]:
        SettingsMultideviceFeatureToggleElement;
  }
  interface HTMLElementEventMap {
    'feature-toggle-clicked':
        CustomEvent<{feature: MultiDeviceFeature, enabled: boolean}>;
  }
}

customElements.define(
    SettingsMultideviceFeatureToggleElement.is,
    SettingsMultideviceFeatureToggleElement);
