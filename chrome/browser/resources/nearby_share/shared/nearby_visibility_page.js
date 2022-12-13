
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-visibility-page' component is part of the Nearby
 * Share onboarding flow. It allows users to setup their visibility preference
 * while enabling the feature for the first time.
 *
 * It is embedded in chrome://os-settings, chrome://settings and as a standalone
 * dialog via chrome://nearby.
 */

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyContactVisibilityElement} from './nearby_contact_visibility.js';
import {NearbyShareOnboardingFinalState, processOnboardingCancelledMetrics, processOnboardingCompleteMetrics, processOnePageOnboardingCancelledMetrics, processOnePageOnboardingCompleteMetrics, processOnePageOnboardingManageContactsMetrics, processOnePageOnboardingVisibilityPageShownMetrics} from './nearby_metrics_logger.js';
import {NearbySettings} from './nearby_share_settings_behavior.js';
import {getTemplate} from './nearby_visibility_page.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NearbyVisibilityPageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class NearbyVisibilityPageElement extends
    NearbyVisibilityPageElementBase {
  static get is() {
    return 'nearby-visibility-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {?NearbySettings} */
      settings: {
        type: Object,
        notify: true,
      },

      /** @private */
      isVisibilitySelected_: {
        type: Boolean,
        notify: true,
      },
    };
  }

  ready() {
    super.ready();
    this.addEventListener('next', this.onNext_);
    this.addEventListener('manage-contacts', this.onManageContacts_);
    this.addEventListener('close', this.onClose_);
    this.addEventListener('view-enter-start', this.onVisibilityViewEnterStart_);
  }

  /**
   * Determines if the feature flag for One-page onboarding workflow is enabled.
   * @return {boolean} whether the one-page onboarding is enabled
   * @private
   */
  isOnePageOnboardingEnabled_() {
    return loadTimeData.getBoolean('isOnePageOnboardingEnabled');
  }

  /** @private */
  onNext_() {
    const contactVisibility = /** @type {NearbyContactVisibilityElement} */
        (this.$.contactVisibility);
    contactVisibility.saveVisibilityAndAllowedContacts();
    this.set('settings.isOnboardingComplete', true);
    this.set('settings.enabled', true);
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingCompleteMetrics(
          NearbyShareOnboardingFinalState.VISIBILITY_PAGE,
          contactVisibility.getSelectedVisibility());
    } else {
      processOnboardingCompleteMetrics();
    }

    const onboardingCompleteEvent = new CustomEvent('onboarding-complete', {
      bubbles: true,
      composed: true,
    });
    this.dispatchEvent(onboardingCompleteEvent);
  }

  /** @private */
  onClose_() {
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingCancelledMetrics(
          NearbyShareOnboardingFinalState.VISIBILITY_PAGE);
    } else {
      processOnboardingCancelledMetrics(
          NearbyShareOnboardingFinalState.VISIBILITY_PAGE);
    }

    const onboardingCancelledEvent = new CustomEvent('onboarding-cancelled', {
      bubbles: true,
      composed: true,
    });
    this.dispatchEvent(onboardingCancelledEvent);
  }

  /** @private */
  onVisibilityViewEnterStart_() {
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingVisibilityPageShownMetrics();
    }
  }

  /** @private */
  onManageContacts_() {
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingManageContactsMetrics();
    }
    window.open(this.i18n('nearbyShareManageContactsUrl'), '_blank');
  }
}

customElements.define(
    NearbyVisibilityPageElement.is, NearbyVisibilityPageElement);
