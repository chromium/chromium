// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-pointers' is the settings subpage with mouse and touchpad settings.
 */
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../../controls/settings_radio_group.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {BatteryStatus, DevicePageBrowserProxy, DevicePageBrowserProxyImpl, ExternalStorage, getDisplayApi, IdleBehavior, LidClosedBehavior, NoteAppInfo, NoteAppLockScreenSupport, PowerManagementSettings, PowerSource, StorageSpaceState} from './device_page_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-pointers',

  behaviors: [
    DeepLinkingBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    hasMouse: Boolean,

    hasPointingStick: Boolean,

    hasTouchpad: Boolean,

    hasHapticTouchpad: Boolean,

    swapPrimaryOptions: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {
            value: false,
            name: loadTimeData.getString('primaryMouseButtonLeft')
          },
          {
            value: true,
            name: loadTimeData.getString('primaryMouseButtonRight')
          },
        ];
      },
    },

    showHeadings_: {
      type: Boolean,
      computed: 'computeShowHeadings_(hasMouse, hasPointingStick, hasTouchpad)',
    },

    subsectionClass_: {
      type: String,
      computed: 'computeSubsectionClass_(hasMouse, hasPointingStick, ' +
          'hasTouchpad)',
    },

    /**
     * TODO(michaelpg): settings-slider should optionally take a min and max so
     * we don't have to generate a simple range of natural numbers ourselves.
     * These values match the TouchpadSensitivity enum in enums.xml.
     * @type {!Array<number>}
     * @private
     */
    sensitivityValues_: {
      type: Array,
      value: [1, 2, 3, 4, 5],
      readOnly: true,
    },

    /**
     * The click sensitivity values from prefs are [1,3,5] but ChromeVox needs
     * to announce them as [1,2,3].
     * @type {!Array<SliderTick>}
     * @private
     */
    hapticClickSensitivityValues_: {
      type: Array,
      value() {
        return [
          {value: 1, ariaValue: 1},
          {value: 3, ariaValue: 2},
          {value: 5, ariaValue: 3},
        ];
      },
      readOnly: true,
    },

    /**
     * TODO(khorimoto): Remove this conditional once the feature is launched.
     * @private
     */
    allowScrollSettings_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowScrollSettings');
      },
    },

    /**
     * TODO(gavinwill): Remove this conditional once the feature is launched.
     * @private
     */
    allowTouchpadHapticFeedback_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowTouchpadHapticFeedback');
      },
    },

    /**
     * TODO(gavinwill): Remove this conditional once the feature is launched.
     * @private
     */
    allowTouchpadHapticClickSettings_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowTouchpadHapticClickSettings');
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kTouchpadTapToClick,
        chromeos.settings.mojom.Setting.kTouchpadTapDragging,
        chromeos.settings.mojom.Setting.kTouchpadReverseScrolling,
        chromeos.settings.mojom.Setting.kTouchpadAcceleration,
        chromeos.settings.mojom.Setting.kTouchpadScrollAcceleration,
        chromeos.settings.mojom.Setting.kTouchpadSpeed,
        chromeos.settings.mojom.Setting.kTouchpadHapticFeedback,
        chromeos.settings.mojom.Setting.kTouchpadHapticClickSensitivity,
        chromeos.settings.mojom.Setting.kPointingStickAcceleration,
        chromeos.settings.mojom.Setting.kPointingStickSpeed,
        chromeos.settings.mojom.Setting.kPointingStickSwapPrimaryButtons,
        chromeos.settings.mojom.Setting.kMouseSwapPrimaryButtons,
        chromeos.settings.mojom.Setting.kMouseReverseScrolling,
        chromeos.settings.mojom.Setting.kMouseAcceleration,
        chromeos.settings.mojom.Setting.kMouseScrollAcceleration,
        chromeos.settings.mojom.Setting.kMouseSpeed,
      ]),
    },
  },

  /**
   * Headings should only be visible if more than one subsection is present.
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   * @param {boolean} hasTouchpad
   * @return {boolean}
   * @private
   */
  computeShowHeadings_(hasMouse, hasPointingStick, hasTouchpad) {
    const sectionVisibilities = [hasMouse, hasPointingStick, hasTouchpad];
    // Count the number of true values in sectionVisibilities.
    const numVisibleSections = sectionVisibilities.filter(x => x).length;
    return numVisibleSections > 1;
  },

  /**
   * Mouse, pointing stick, and touchpad sections are only subsections if more
   * than one is present.
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   * @param {boolean} hasTouchpad
   * @return {string}
   * @private
   */
  computeSubsectionClass_(hasMouse, hasPointingStick, hasTouchpad) {
    const subsections =
        this.computeShowHeadings_(hasMouse, hasPointingStick, hasTouchpad);
    return subsections ? 'subsection' : '';
  },

  /**
   * @param {!Route} route
   * @param {Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.POINTERS) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreLinkClicked_: function(event) {
    if (!Array.isArray(event.path) || !event.path.length) {
      return;
    }

    if (event.path[0].tagName === 'A') {
      // Do not toggle reverse scrolling if the contained link is clicked.
      event.stopPropagation();
    }
  },

  /** @private */
  onMouseReverseScrollRowClicked_: function() {
    this.setPrefValue(
        'settings.mouse.reverse_scroll',
        !this.getPref('settings.mouse.reverse_scroll').value);
  },

  /** @private */
  onTouchpadReverseScrollRowClicked_: function() {
    this.setPrefValue(
        'settings.touchpad.natural_scroll',
        !this.getPref('settings.touchpad.natural_scroll').value);
  },

  /** @private */
  onTouchpadHapticFeedbackRowClicked_: function() {
    this.setPrefValue(
        'settings.touchpad.haptic_feedback',
        !this.getPref('settings.touchpad.haptic_feedback').value);
  },
});
