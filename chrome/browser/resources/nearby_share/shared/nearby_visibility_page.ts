
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

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import './nearby_contact_visibility.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {NearbyContactVisibilityElement} from './nearby_contact_visibility.js';
import {getOnboardingEntryPoint, NearbyShareOnboardingEntryPoint, NearbyShareOnboardingFinalState, processOnboardingCancelledMetrics, processOnboardingCompleteMetrics, processOnePageOnboardingCancelledMetrics, processOnePageOnboardingCompleteMetrics, processOnePageOnboardingManageContactsMetrics, processOnePageOnboardingVisibilityPageShownMetrics} from './nearby_metrics_logger.js';
import type {NearbySettings} from './nearby_share_settings_mixin.js';
import {getTemplate} from './nearby_visibility_page.html.js';

export interface NearbyVisibilityPageElement {
  $: {
    contactVisibility: NearbyContactVisibilityElement,
  };
}

const NearbyVisibilityPageElementBase = I18nMixin(PolymerElement);

export class NearbyVisibilityPageElement extends
    NearbyVisibilityPageElementBase {
  static get is() {
    return 'nearby-visibility-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      settings: {
        type: Object,
        notify: true,
      },

      isVisibilitySelected_: {
        type: Boolean,
        notify: true,
      },

      /**
       * Onboarding page entry point
       */
      entryPoint_: {
        type: NearbyShareOnboardingEntryPoint,
        value: NearbyShareOnboardingEntryPoint.MAX,
      },

    };
  }

  settings: NearbySettings;
  private isVisibilitySelected_: boolean;
  private entryPoint_: NearbyShareOnboardingEntryPoint;

  override ready(): void {
    super.ready();
    this.addEventListener('next', this.onNext_);
    this.addEventListener('manage-contacts', this.onManageContacts_);
    this.addEventListener('close', this.onClose_);
    this.addEventListener('view-enter-start', this.onVisibilityViewEnterStart_);
  }

  /**
   * Determines if the feature flag for One-page onboarding workflow is enabled.
   */
  private isOnePageOnboardingEnabled_(): boolean {
    return loadTimeData.getBoolean('isOnePageOnboardingEnabled');
  }

  private onNext_(): void {
    this.$.contactVisibility.saveVisibilityAndAllowedContacts();
    this.set('settings.isOnboardingComplete', true);
    this.set('settings.enabled', true);
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingCompleteMetrics(
          this.entryPoint_, NearbyShareOnboardingFinalState.VISIBILITY_PAGE,
          this.$.contactVisibility.getSelectedVisibility());
    } else {
      processOnboardingCompleteMetrics(this.entryPoint_);
    }

    const onboardingCompleteEvent = new CustomEvent('onboarding-complete', {
      bubbles: true,
      composed: true,
    });
    this.dispatchEvent(onboardingCompleteEvent);
  }

  private onClose_(): void {
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingCancelledMetrics(
          this.entryPoint_, NearbyShareOnboardingFinalState.VISIBILITY_PAGE);
    } else {
      processOnboardingCancelledMetrics(
          this.entryPoint_, NearbyShareOnboardingFinalState.VISIBILITY_PAGE);
    }

    const onboardingCancelledEvent = new CustomEvent('onboarding-cancelled', {
      bubbles: true,
      composed: true,
    });
    this.dispatchEvent(onboardingCancelledEvent);
  }

  private onVisibilityViewEnterStart_(): void {
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingVisibilityPageShownMetrics();
    }
    const url: URL = new URL(document.URL);
    this.entryPoint_ = getOnboardingEntryPoint(url);
  }

  private onManageContacts_(): void {
    if (this.isOnePageOnboardingEnabled_()) {
      processOnePageOnboardingManageContactsMetrics();
    }
    window.open(this.i18n('nearbyShareManageContactsUrl'), '_blank');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NearbyVisibilityPageElement.is]: NearbyVisibilityPageElement;
  }
}

customElements.define(
    NearbyVisibilityPageElement.is, NearbyVisibilityPageElement);
