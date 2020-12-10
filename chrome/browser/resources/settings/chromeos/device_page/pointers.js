// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-pointers' is the settings subpage with mouse and touchpad settings.
 */
Polymer({
  is: 'settings-pointers',

  behaviors: [
    DeepLinkingBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    hasMouse: Boolean,

    hasPointingStick: Boolean,

    hasTouchpad: Boolean,

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

    /**
     * Interim property for use until we have a separate subsection for pointing
     * sticks. (See crbug.com/1114828)
     * @private
     */
    showMouseSection_: {
      type: Boolean,
      computed: 'computeShowMouseSection_(separatePointingStickSettings_, ' +
          'hasMouse, hasPointingStick)',
    },

    showHeadings_: {
      type: Boolean,
      computed: 'computeShowHeadings_(separatePointingStickSettings_, ' +
          'hasMouse, hasPointingStick, hasTouchpad)',
    },

    subsectionClass_: {
      type: String,
      computed: 'computeSubsectionClass_(separatePointingStickSettings_, ' +
          'hasMouse, hasPointingStick, hasTouchpad)',
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
     * TODO(zentaro): Remove this conditional once the feature is launched.
     * @private
     */
    allowDisableAcceleration_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowDisableMouseAcceleration');
      },
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
     * TODO(crbug.com/1114828): Remove this conditional once the feature is
     * launched.
     * @private
     */
    separatePointingStickSettings_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('separatePointingStickSettings');
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
   * @param {boolean} separateSettings
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   */
  computeShowMouseSection_(separateSettings, hasMouse, hasPointingStick) {
    return separateSettings ? hasMouse : hasMouse || hasPointingStick;
  },

  /**
   * Headings should only be visible if more than one subsection is present.
   * @param {boolean} separateSettings
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   * @param {boolean} hasTouchpad
   * @return {boolean}
   * @private
   */
  computeShowHeadings_(
      separateSettings, hasMouse, hasPointingStick, hasTouchpad) {
    if (!separateSettings) {
      return (hasMouse || hasPointingStick) && hasTouchpad;
    }
    const sectionVisibilities = [hasMouse, hasPointingStick, hasTouchpad];
    // Count the number of true values in sectionVisibilities.
    const numVisibleSections = sectionVisibilities.filter(x => x).length;
    return numVisibleSections > 1;
  },

  /**
   * Mouse, pointing stick, and touchpad sections are only subsections if more
   * than one is present.
   * @param {boolean} separateSettings
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   * @param {boolean} hasTouchpad
   * @return {string}
   * @private
   */
  computeSubsectionClass_(
      separateSettings, hasMouse, hasPointingStick, hasTouchpad) {
    const subsections = this.computeShowHeadings_(
        separateSettings, hasMouse, hasPointingStick, hasTouchpad);
    return subsections ? 'subsection' : '';
  },

  /**
   * @param {!settings.Route} route
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.POINTERS) {
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
  onMousePrimaryButtonChanged_: function() {
    if (!loadTimeData.getBoolean('separatePointingStickSettings')) {
      // To preserve the old behaviour, set pointing stick button too.
      // TODO(crbug.com/1114828): remove once the feature is launched.
      const value = this.getPref('settings.mouse.primary_right').value;
      this.setPrefValue('settings.pointing_stick.primary_right', value);
    }
  },

  /** @private */
  onMouseAccelerationChanged_: function() {
    if (!loadTimeData.getBoolean('separatePointingStickSettings')) {
      // To preserve the old behaviour, set pointing stick acceleration too.
      // TODO(crbug.com/1114828): remove once the feature is launched.
      const value = this.getPref('settings.mouse.acceleration').value;
      this.setPrefValue('settings.pointing_stick.acceleration', value);
    }
  },

  /** @private */
  onMouseSpeedChanged_: function() {
    if (!loadTimeData.getBoolean('separatePointingStickSettings')) {
      // To preserve the old behaviour, set the pointing stick speed to match.
      // TODO(crbug.com/1114828): remove once the feature is launched.
      const value = this.getPref('settings.mouse.sensitivity2').value;
      this.setPrefValue('settings.pointing_stick.sensitivity', value);
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
});
