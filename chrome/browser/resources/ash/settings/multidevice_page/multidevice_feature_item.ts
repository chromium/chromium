// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item for an individual multidevice feature. These features appear in the
 * multidevice subpage to allow the user to individually toggle them as long as
 * the phone is enabled as a multidevice host. The feature items contain basic
 * information relevant to the individual feature, such as a route to the
 * feature's autonomous page if there is one.
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import './multidevice_feature_toggle.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {Route, Router, routes} from '../router.js';

import {MultiDeviceFeature} from './multidevice_constants.js';
import {getTemplate} from './multidevice_feature_item.html.js';
import {MultiDeviceFeatureMixin} from './multidevice_feature_mixin.js';
import {recordSmartLockToggleMetric, SmartLockToggleLocation} from './multidevice_metrics_logger.js';

const SettingsMultideviceFeatureItemElementBase =
    MultiDeviceFeatureMixin(RouteOriginMixin(PolymerElement));

export class SettingsMultideviceFeatureItemElement extends
    SettingsMultideviceFeatureItemElementBase {
  static get is() {
    return 'settings-multidevice-feature-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      feature: Number,

      /**
       * If it is truthy, the item should be actionable and clicking on it
       * should navigate to the provided route. Otherwise, the item does not
       * have a subpage to navigate to.
       */
      subpageRoute: Object,

      /**
       * A tooltip to show over a help icon. If unset, no help icon is shown.
       */
      iconTooltip: String,

      /**
       * A Chrome icon asset to use as a help icon. The icon is not shown if
       * iconTooltip is unset. Defaults to cr:info-outline.
       */
      icon: {
        type: String,
        value: 'cr:info-outline',
      },

      /**
       * URLSearchParams for subpage route. No param is provided if it is
       * undefined.
       */
      subpageRouteUrlSearchParams: Object,

      /** Whether if the feature is a sub-feature */
      isSubFeature: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** Whether feature icon is present next to text in row */
      isFeatureIconHidden: {
        type: Boolean,
        value: false,
      },
    };
  }

  feature: MultiDeviceFeature;
  icon: string;
  iconTooltip: string;
  isSubFeature: boolean;
  isFeatureIconHidden: boolean;
  subpageRoute: Route|undefined;
  subpageRouteUrlSearchParams: URLSearchParams|undefined;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.MULTIDEVICE_FEATURES;
  }

  override ready(): void {
    super.ready();

    this.addEventListener(
        'feature-toggle-clicked', this.onFeatureToggleClicked_);

    this.addFocusConfig(this.subpageRoute, '#subpageButton');
  }

  override focus(): void {
    const slot = castExists(this.shadowRoot!.querySelector<HTMLSlotElement>(
        'slot[name="feature-controller"]'));
    const elems = slot.assignedElements({flatten: true});
    assert(elems.length > 0);
    // Elems contains any elements that override the feature controller. If none
    // exist, contains the default toggle elem.
    (elems[0] as HTMLElement).focus();
  }

  private isRowClickable_(): boolean {
    return this.hasSubpageClickHandler_() ||
        this.isFeatureStateEditable(this.feature);
  }

  private hasSubpageClickHandler_(): boolean {
    return !!this.subpageRoute && this.isFeatureAllowedByPolicy(this.feature);
  }

  private shouldShowSeparator_(): boolean {
    return this.hasSubpageClickHandler_() || !!this.iconTooltip;
  }

  private handleItemClick_(event: Event): void {
    // We do not navigate away if the click was on a link.
    if ((event.composedPath()[0] as Element).tagName === 'A') {
      event.stopPropagation();
      return;
    }

    if (!this.hasSubpageClickHandler_()) {
      if (this.isFeatureStateEditable(this.feature)) {
        // Toggle the editable feature if the feature is editable and does not
        // link to a subpage.
        const toggleButton = castExists(this.shadowRoot!.querySelector(
            'settings-multidevice-feature-toggle'));
        toggleButton.toggleFeature();
      }
      return;
    }

    // Remove the search term when navigating to avoid potentially having any
    // visible search term reappear at a later time. See
    // https://crbug.com/989119.
    assert(this.subpageRoute);
    Router.getInstance().navigateTo(
        this.subpageRoute, this.subpageRouteUrlSearchParams,
        true /* opt_removeSearch */);
  }


  /**
   * The class name used for given multidevice feature item text container
   * Checks if icon is present next to text to determine if class 'middle'
   * applies
   */
  private getItemTextContainerClassName_(isFeatureIconHidden: boolean): string {
    return isFeatureIconHidden ? 'start' : 'middle';
  }

  /**
   * Intercept (but do not stop propagation of) the feature-toggle-clicked event
   * for the purpose of logging metrics.
   */
  private onFeatureToggleClicked_(
      event: CustomEvent<{feature: MultiDeviceFeature, enabled: boolean}>):
      void {
    const feature = event.detail.feature;
    const enabled = event.detail.enabled;

    if (feature === MultiDeviceFeature.SMART_LOCK) {
      const toggleLocation =
          Router.getInstance().currentRoute.contains(routes.LOCK_SCREEN) ?
          SmartLockToggleLocation.LOCK_SCREEN_SETTINGS :
          SmartLockToggleLocation.MULTIDEVICE_PAGE;

      recordSmartLockToggleMetric(toggleLocation, enabled);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultideviceFeatureItemElement.is]:
        SettingsMultideviceFeatureItemElement;
  }
}

customElements.define(
    SettingsMultideviceFeatureItemElement.is,
    SettingsMultideviceFeatureItemElement);
