// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-pointers' is the settings subpage with mouse and touchpad settings.
 */
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_radio_group.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isInputDeviceSettingsSplitEnabled} from '../common/load_time_booleans.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './pointers.html.js';

const SettingsPointersElementBase =
    DeepLinkingMixin(RouteObserverMixin(PrefsMixin(I18nMixin(PolymerElement))));

export class SettingsPointersElement extends SettingsPointersElementBase {
  static get is() {
    return 'settings-pointers';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
              name: loadTimeData.getString('primaryMouseButtonLeft'),
            },
            {
              value: true,
              name: loadTimeData.getString('primaryMouseButtonRight'),
            },
          ];
        },
      },

      showHeadings_: {
        type: Boolean,
        computed:
            'computeShowHeadings_(hasMouse, hasPointingStick, hasTouchpad)',
      },

      subsectionClass_: {
        type: String,
        computed: 'computeSubsectionClass_(hasMouse, hasPointingStick, ' +
            'hasTouchpad)',
      },

      /**
       * TODO(michaelpg): settings-slider should optionally take a min and max
       * so we don't have to generate a simple range of natural numbers
       * ourselves. These values match the TouchpadSensitivity enum in
       * enums.xml.
       */
      sensitivityValues_: {
        type: Array,
        value: [1, 2, 3, 4, 5],
        readOnly: true,
      },

      /**
       * The click sensitivity values from prefs are [1,3,5] but ChromeVox needs
       * to announce them as [1,2,3].
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
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kTouchpadTapToClick,
          Setting.kTouchpadTapDragging,
          Setting.kTouchpadReverseScrolling,
          Setting.kTouchpadAcceleration,
          Setting.kTouchpadSpeed,
          Setting.kTouchpadHapticFeedback,
          Setting.kTouchpadHapticClickSensitivity,
          Setting.kPointingStickAcceleration,
          Setting.kPointingStickSpeed,
          Setting.kPointingStickSwapPrimaryButtons,
          Setting.kMouseSwapPrimaryButtons,
          Setting.kMouseReverseScrolling,
          Setting.kMouseAcceleration,
          Setting.kMouseSpeed,
        ]),
      },

      /**
       * Whether settings should be split per device.
       */
      isDeviceSettingsSplitEnabled_: {
        type: Boolean,
        value() {
          return isInputDeviceSettingsSplitEnabled();
        },
        readOnly: true,
      },
    };
  }

  hasMouse: boolean;
  hasPointingStick: boolean;
  hasTouchpad: boolean;
  hasHapticTouchpad: boolean;
  private isDeviceSettingsSplitEnabled_: boolean;

  /**
   * Headings should only be visible if more than one subsection is present.
   */
  private computeShowHeadings_(
      hasMouse: boolean, hasPointingStick: boolean,
      hasTouchpad: boolean): boolean {
    const sectionVisibilities = [hasMouse, hasPointingStick, hasTouchpad];
    // Count the number of true values in sectionVisibilities.
    const numVisibleSections = sectionVisibilities.filter(x => x).length;
    return numVisibleSections > 1;
  }

  /**
   * Mouse, pointing stick, and touchpad sections are only subsections if more
   * than one is present.
   */
  private computeSubsectionClass_(
      hasMouse: boolean, hasPointingStick: boolean,
      hasTouchpad: boolean): string {
    const subsections =
        this.computeShowHeadings_(hasMouse, hasPointingStick, hasTouchpad);
    return subsections ? 'subsection' : '';
  }

  private getCursorSpeedString(): TrustedHTML {
    return this.i18nAdvanced(
        loadTimeData.getBoolean('allowScrollSettings') ? 'cursorSpeed' :
                                                         'mouseSpeed');
  }

  private getCursorAccelerationString(): TrustedHTML {
    return this.i18nAdvanced(
        loadTimeData.getBoolean('allowScrollSettings') ?
            'cursorAccelerationLabel' :
            'mouseAccelerationLabel');
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.POINTERS) {
      return;
    }
    if (Router.getInstance().currentRoute === routes.POINTERS &&
        this.isDeviceSettingsSplitEnabled_) {
      // Call setCurrentRoute function to go to the device page when
      // the feature flag is turned on. We don't use navigateTo function since
      // we don't want to navigate back to the previous point page.
      setTimeout(() => {
        Router.getInstance().setCurrentRoute(
            routes.DEVICE, new URLSearchParams(), false);
      });
    }
    this.attemptDeepLink();
  }

  private onLearnMoreLinkClicked_(event: Event): void {
    const path = event.composedPath();
    if (!Array.isArray(path) || !path.length) {
      return;
    }

    if ((path[0] as HTMLElement).tagName === 'A') {
      // Do not toggle reverse scrolling if the contained link is clicked.
      event.stopPropagation();
    }
  }

  private onMouseReverseScrollRowClicked_(): void {
    this.setPrefValue(
        'settings.mouse.reverse_scroll',
        !this.getPref('settings.mouse.reverse_scroll').value);
  }

  private onTouchpadReverseScrollRowClicked_(): void {
    this.setPrefValue(
        'settings.touchpad.natural_scroll',
        !this.getPref('settings.touchpad.natural_scroll').value);
  }

  private onTouchpadHapticFeedbackRowClicked_(): void {
    this.setPrefValue(
        'settings.touchpad.haptic_feedback',
        !this.getPref('settings.touchpad.haptic_feedback').value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-pointers': SettingsPointersElement;
  }
}

customElements.define(SettingsPointersElement.is, SettingsPointersElement);
