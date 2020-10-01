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

    /**
     * Interim property for use until we have a separate subsection for pointing
     * sticks. (See crbug.com/1114828)
     * @private
     */
    showMouseSection_: {
      type: Boolean,
      computed: 'computeShowMouseSection_(hasMouse, hasPointingStick)',
    },

    /**
     * TODO(michaelpg): settings-slider should optionally take a min and max so
     * we don't have to generate a simple range of natural numbers ourselves.
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
        chromeos.settings.mojom.Setting.kMouseSwapPrimaryButtons,
        chromeos.settings.mojom.Setting.kMouseReverseScrolling,
        chromeos.settings.mojom.Setting.kMouseAcceleration,
        chromeos.settings.mojom.Setting.kMouseScrollAcceleration,
        chromeos.settings.mojom.Setting.kMouseSpeed,
      ]),
    },
  },

  /**
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   */
  computeShowMouseSection_(hasMouse, hasPointingStick) {
    return hasMouse || hasPointingStick;
  },

  // Used to correctly identify when the mouse button has been released.
  // crbug.com/686949.
  receivedMouseSwapButtonsDown_: false,

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
   * Mouse and touchpad sections are only subsections if they are both present.
   * @param {boolean} showMouseSection
   * @param {boolean} hasTouchpad
   * @return {string}
   * @private
   */
  getSubsectionClass_(showMouseSection, hasTouchpad) {
    return showMouseSection && hasTouchpad ? 'subsection' : '';
  },

  /** @private */
  onMouseSwapButtonsDown_() {
    this.receivedMouseSwapButtonsDown_ = true;
  },

  /** @private */
  onMouseSwapButtonsUp_() {
    this.receivedMouseSwapButtonsDown_ = false;
    /** @type {!SettingsToggleButtonElement} */ (this.$.mouseSwapButton)
        .sendPrefChange();
  },

  /** @private */
  onMouseSwapButtonsChange_() {
    if (!this.receivedMouseSwapButtonsDown_) {
      /** @type {!SettingsToggleButtonElement} */ (this.$.mouseSwapButton)
          .sendPrefChange();
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreLinkClicked_: function(event) {
    if (!Array.isArray(event.path) || !event.path.length) {
      return;
    }

    if (event.path[0].tagName == 'A') {
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
});
