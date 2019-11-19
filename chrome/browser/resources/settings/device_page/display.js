// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display' is the settings subpage for display settings.
 */

/**
 * The types of Night Light automatic schedule. The values of the enum values
 * are synced with the pref "prefs.ash.night_light.schedule_type".
 * @enum {number}
 */
const NightLightScheduleType = {
  NEVER: 0,
  SUNSET_TO_SUNRISE: 1,
  CUSTOM: 2,
};

/**
 * @typedef {{
 *   value: (!{
 *     recommended: (boolean|undefined),
 *     external_width: (number|undefined),
 *     external_height: (number|undefined),
 *     external_use_native: (boolean|undefined),
 *     external_scale_percentage: (number|undefined),
 *     internal_scale_percentage: (number|undefined)
 *   }|null)
 * }}
 */
let DisplayResolutionPrefObject;

cr.define('settings.display', function() {
  const systemDisplayApi =
      /** @type {!SystemDisplay} */ (chrome.system.display);

  return {
    systemDisplayApi: systemDisplayApi,
  };
});

Polymer({
  is: 'settings-display',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    /**
     * @type {!chrome.settingsPrivate.PrefObject}
     * @private
     */
    selectedModePref_: {
      type: Object,
      value: function() {
        return {
          key: 'fakeDisplaySliderPref',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        };
      },
    },

    /**
     * @type {!chrome.settingsPrivate.PrefObject}
     * @private
     */
    selectedZoomPref_: {
      type: Object,
      value: function() {
        return {
          key: 'fakeDisplaySliderZoomPref',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: 0,
        };
      },
    },

    /**
     * Array of displays.
     * @type {!Array<!chrome.system.display.DisplayUnitInfo>}
     */
    displays: Array,

    /**
     * Array of display layouts.
     * @type {!Array<!chrome.system.display.DisplayLayout>}
     */
    layouts: Array,

    /**
     * String listing the ids in displays. Used to observe changes to the
     * display configuration (i.e. when a display is added or removed).
     */
    displayIds: {type: String, observer: 'onDisplayIdsChanged_'},

    /** Primary display id */
    primaryDisplayId: String,

    /** @type {!chrome.system.display.DisplayUnitInfo|undefined} */
    selectedDisplay: Object,

    /** Id passed to the overscan dialog. */
    overscanDisplayId: {
      type: String,
      notify: true,
    },

    /** Ids for mirroring destination displays. */
    mirroringDestinationIds: Array,

    /** @private {!Array<number>} Mode index values for slider. */
    modeValues_: Array,

    /**
     * @private {!Array<cr_slider.SliderTick>} Display zoom slider tick values.
     */
    zoomValues_: Array,

    /** @private {!DropdownMenuOptionList} */
    displayModeList_: {
      type: Array,
      value: [],
    },

    /** @private */
    unifiedDesktopAvailable_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('unifiedDesktopAvailable');
      }
    },

    /** @private */
    ambientColorAvailable_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('deviceSupportsAmbientColor');
      }
    },

    /** @private */
    listAllDisplayModes_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('listAllDisplayModes');
      }
    },

    /** @private */
    unifiedDesktopMode_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    scheduleTypesList_: {
      type: Array,
      value: function() {
        return [
          {
            name: loadTimeData.getString('displayNightLightScheduleNever'),
            value: NightLightScheduleType.NEVER
          },
          {
            name: loadTimeData.getString(
                'displayNightLightScheduleSunsetToSunRise'),
            value: NightLightScheduleType.SUNSET_TO_SUNRISE
          },
          {
            name: loadTimeData.getString('displayNightLightScheduleCustom'),
            value: NightLightScheduleType.CUSTOM
          }
        ];
      },
    },

    /** @private */
    shouldOpenCustomScheduleCollapse_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    nightLightScheduleSubLabel_: String,

    /** @private */
    logicalResolutionText_: String,

    /** @private {!Array<string>} */
    displayTabNames_: Array,

    /** @private */
    selectedTab_: Number,
  },

  observers: [
    'updateNightLightScheduleSettings_(prefs.ash.night_light.schedule_type.*,' +
        ' prefs.ash.night_light.enabled.*)',
    'onSelectedModeChange_(selectedModePref_.value)',
    'onSelectedZoomChange_(selectedZoomPref_.value)',
    'onDisplaysChanged_(displays.*)',
  ],

  /** @private {number} Selected mode index received from chrome. */
  currentSelectedModeIndex_: -1,

  /**
   * Listener for chrome.system.display.onDisplayChanged events.
   * @type {function(void)|undefined}
   * @private
   */
  displayChangedListener_: undefined,

  /** @override */
  attached: function() {
    this.displayChangedListener_ =
        this.displayChangedListener_ || this.getDisplayInfo_.bind(this);
    settings.display.systemDisplayApi.onDisplayChanged.addListener(
        this.displayChangedListener_);

    this.getDisplayInfo_();
    this.$.displaySizeSlider.updateValueInstantly = false;
  },

  /** @override */
  detached: function() {
    settings.display.systemDisplayApi.onDisplayChanged.removeListener(
        assert(this.displayChangedListener_));

    this.currentSelectedModeIndex_ = -1;
  },

  /**
   * Shows or hides the overscan dialog.
   * @param {boolean} showOverscan
   * @private
   */
  showOverscanDialog_: function(showOverscan) {
    if (showOverscan) {
      this.$.displayOverscan.open();
      this.$.displayOverscan.focus();
    } else {
      this.$.displayOverscan.close();
    }
  },

  /** @private */
  onDisplayIdsChanged_: function() {
    // Close any overscan dialog (which will cancel any overscan operation)
    // if displayIds changes.
    this.showOverscanDialog_(false);
  },

  /** @private */
  getDisplayInfo_: function() {
    /** @type {chrome.system.display.GetInfoFlags} */ const flags = {
      singleUnified: true
    };
    settings.display.systemDisplayApi.getInfo(
        flags, this.displayInfoFetched_.bind(this));
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @private
   */
  displayInfoFetched_: function(displays) {
    if (!displays.length) {
      return;
    }
    settings.display.systemDisplayApi.getDisplayLayout(
        this.displayLayoutFetched_.bind(this, displays));
    if (this.isMirrored_(displays)) {
      this.mirroringDestinationIds = displays[0].mirroringDestinationIds;
    } else {
      this.mirroringDestinationIds = [];
    }
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @param {!Array<!chrome.system.display.DisplayLayout>} layouts
   * @private
   */
  displayLayoutFetched_: function(displays, layouts) {
    this.layouts = layouts;
    this.displays = displays;
    this.displayTabNames_ = displays.map(({name}) => name);
    this.updateDisplayInfo_();
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {number} The index of the currently selected mode of the
   * |selectedDisplay|. If the display has no modes, returns 0.
   * @private
   */
  getSelectedModeIndex_: function(selectedDisplay) {
    for (let i = 0; i < selectedDisplay.modes.length; ++i) {
      if (selectedDisplay.modes[i].isSelected) {
        return i;
      }
    }
    return 0;
  },

  /**
   * Checks if the given device policy is enabled.
   * @param {DisplayResolutionPrefObject} policyPref
   * @return {boolean}
   * @private
   */
  isDevicePolicyEnabled_: function(policyPref) {
    return policyPref !== undefined && policyPref.value !== null;
  },

  /**
   * Checks if display resolution is managed by device policy.
   * @param {DisplayResolutionPrefObject} resolutionPref
   * @return {boolean}
   * @private
   */
  isDisplayResolutionManagedByPolicy_: function(resolutionPref) {
    return this.isDevicePolicyEnabled_(resolutionPref) &&
        (resolutionPref.value.external_use_native !== undefined ||
         (resolutionPref.value.external_width !== undefined &&
          resolutionPref.value.external_height !== undefined));
  },

  /**
   * Checks if display resolution is managed by policy and the policy
   * is mandatory.
   * @param {DisplayResolutionPrefObject} resolutionPref
   * @return {boolean}
   * @private
   */
  isDisplayResolutionMandatory_: function(resolutionPref) {
    return this.isDisplayResolutionManagedByPolicy_(resolutionPref) &&
        !resolutionPref.value.recommended;
  },

  /**
   * Checks if display scale factor is managed by device policy.
   * @param {chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @param {DisplayResolutionPrefObject} resolutionPref
   * @return {boolean}
   * @private
   */
  isDisplayScaleManagedByPolicy_: function(selectedDisplay, resolutionPref) {
    if (!this.isDevicePolicyEnabled_(resolutionPref) || !selectedDisplay) {
      return false;
    }
    if (selectedDisplay.isInternal) {
      return resolutionPref.value.internal_scale_percentage !== undefined;
    }
    return resolutionPref.value.external_scale_percentage !== undefined;
  },

  /**
   * Checks if display scale factor is managed by policy and the policy
   * is mandatory.
   * @param {DisplayResolutionPrefObject} resolutionPref
   * @return {boolean}
   * @private
   */
  isDisplayScaleMandatory_: function(selectedDisplay, resolutionPref) {
    return this.isDisplayScaleManagedByPolicy_(
               selectedDisplay, resolutionPref) &&
        !resolutionPref.value.recommended;
  },

  /**
   * Returns the list of display modes that is shown to the user in a drop down
   * menu.
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {!DropdownMenuOptionList}
   * @private
   */
  getDisplayModeOptionList_: function(selectedDisplay) {
    const optionList = [];

    const listAllModes = this.listAllDisplayModes_;

    for (let i = 0; i < selectedDisplay.modes.length; ++i) {
      const mode = selectedDisplay.modes[i];

      const id = listAllModes && mode.isInterlaced ?
          'displayResolutionInterlacedMenuItem' :
          'displayResolutionMenuItem';
      const refreshRate = Math.round(mode.refreshRate * 100) / 100;
      const option = this.i18n(
          id, mode.width.toString(), mode.height.toString(),
          refreshRate.toString());

      optionList.push({
        name: option,
        value: i,
      });
    }
    return optionList;
  },

  /**
   * Returns a value from |zoomValues_| that is closest to the display zoom
   * percentage currently selected for the |selectedDisplay|.
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {number}
   * @private
   */
  getSelectedDisplayZoom_: function(selectedDisplay) {
    const selectedZoom = selectedDisplay.displayZoomFactor;
    let closestMatch = this.zoomValues_[0].value;
    let minimumDiff = Math.abs(closestMatch - selectedZoom);

    for (let i = 0; i < this.zoomValues_.length; i++) {
      const currentDiff = Math.abs(this.zoomValues_[i].value - selectedZoom);
      if (currentDiff < minimumDiff) {
        closestMatch = this.zoomValues_[i].value;
        minimumDiff = currentDiff;
      }
    }

    return /** @type {number} */ (closestMatch);
  },

  /**
   * Given the display with the current display mode, this function lists all
   * the display zoom values and their labels to be used by the slider.
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {!Array<cr_slider.SliderTick>}
   */
  getZoomValues_: function(selectedDisplay) {
    return selectedDisplay.availableDisplayZoomFactors.map(value => {
      const ariaValue = Math.round(value * 100);
      return {
        value,
        ariaValue,
        label: this.i18n('displayZoomValue', ariaValue.toString())
      };
    });
  },

  /**
   * We need to call this explicitly rather than relying on change events
   * so that we can control the update order.
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @private
   */
  setSelectedDisplay_: function(selectedDisplay) {
    // |modeValues_| controls the resolution slider's tick values. Changing it
    // might trigger a change in the |selectedModePref_.value| if the number of
    // modes differs and the current mode index is out of range of the new modes
    // indices. Thus, we need to set |currentSelectedModeIndex_| to -1 to
    // indicate that the |selectedDisplay| and |selectedModePref_.value| are out
    // of sync, and therefore getResolutionText_() and onSelectedModeChange_()
    // will be no-ops.
    this.currentSelectedModeIndex_ = -1;
    const numModes = selectedDisplay.modes.length;
    this.modeValues_ = numModes == 0 ? [] : Array.from(Array(numModes).keys());

    // Note that the display zoom values has the same number of ticks for all
    // displays, so the above problem doesn't apply here.
    this.zoomValues_ = this.getZoomValues_(selectedDisplay);
    this.set(
        'selectedZoomPref_.value',
        this.getSelectedDisplayZoom_(selectedDisplay));

    this.displayModeList_ = this.getDisplayModeOptionList_(selectedDisplay);
    // Set |selectedDisplay| first since only the resolution slider depends
    // on |selectedModePref_|.
    this.selectedDisplay = selectedDisplay;
    this.selectedTab_ = this.displays.indexOf(this.selectedDisplay);

    // Now that everything is in sync, set the selected mode to its correct
    // value right before updating the pref.
    this.currentSelectedModeIndex_ =
        this.getSelectedModeIndex_(selectedDisplay);
    this.set('selectedModePref_.value', this.currentSelectedModeIndex_);

    this.updateLogicalResolutionText_(
        /** @type {number} */ (this.selectedZoomPref_.value));
  },

  /**
   * Returns true if the resolution setting needs to be displayed.
   * @param {!chrome.system.display.DisplayUnitInfo} display
   * @return {boolean}
   * @private
   */
  showDropDownResolutionSetting_: function(display) {
    return !display.isInternal;
  },

  /**
   * Returns true if external touch devices are connected and the current
   * display is not an internal display. If the feature is not enabled via the
   * switch, this will return false.
   * @param {!chrome.system.display.DisplayUnitInfo} display Display being
   *     checked for touch support.
   * @return {boolean}
   * @private
   */
  showTouchCalibrationSetting_: function(display) {
    return !display.isInternal &&
        loadTimeData.getBoolean('hasExternalTouchDevice') &&
        loadTimeData.getBoolean('enableTouchCalibrationSetting');
  },

  /**
   * Returns true if the overscan setting should be shown for |display|.
   * @param {!chrome.system.display.DisplayUnitInfo} display
   * @return {boolean}
   * @private
   */
  showOverscanSetting_: function(display) {
    return !display.isInternal;
  },

  /**
   * Returns true if the ambient color setting should be shown for |display|.
   * @param {boolean} ambientColorAvailable
   * @param {chrome.system.display.DisplayUnitInfo} display
   * @return {boolean}
   * @private
   */
  showAmbientColorSetting_: function(ambientColorAvailable, display) {
    return ambientColorAvailable && display && display.isInternal;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasMultipleDisplays_: function() {
    return this.displays.length > 1;
  },

  /**
   * Returns false if the display select menu has to be hidden.
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  showDisplaySelectMenu_: function(displays, selectedDisplay) {
    if (selectedDisplay) {
      return displays.length > 1 && !selectedDisplay.isPrimary;
    }

    return false;
  },

  /**
   * Returns the select menu index indicating whether the display currently is
   * primary or extended.
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @param {string} primaryDisplayId
   * @return {number} Returns 0 if the display is primary else returns 1.
   * @private
   */
  getDisplaySelectMenuIndex_: function(selectedDisplay, primaryDisplayId) {
    if (selectedDisplay && selectedDisplay.id == primaryDisplayId) {
      return 0;
    }
    return 1;
  },

  /**
   * Returns the i18n string for the text to be used for mirroring settings.
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {string} i18n string for mirroring settings text.
   * @private
   */
  getDisplayMirrorText_: function(displays) {
    return this.i18n('displayMirror', displays[0].name);
  },

  /**
   * @param {boolean} unifiedDesktopAvailable
   * @param {boolean} unifiedDesktopMode
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {boolean}
   * @private
   */
  showUnifiedDesktop_: function(
      unifiedDesktopAvailable, unifiedDesktopMode, displays) {
    if (displays === undefined) {
      return false;
    }

    return unifiedDesktopMode ||
        (unifiedDesktopAvailable && displays.length > 1 &&
         !this.isMirrored_(displays));
  },

  /**
   * @param {boolean} unifiedDesktopMode
   * @return {string}
   * @private
   */
  getUnifiedDesktopText_: function(unifiedDesktopMode) {
    return this.i18n(unifiedDesktopMode ? 'toggleOn' : 'toggleOff');
  },

  /**
   * @param {boolean} unifiedDesktopMode
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {boolean}
   * @private
   */
  showMirror_: function(unifiedDesktopMode, displays) {
    if (displays === undefined) {
      return false;
    }

    return this.isMirrored_(displays) ||
        (!unifiedDesktopMode && displays.length > 1);
  },

  /**
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @return {boolean}
   * @private
   */
  isMirrored_: function(displays) {
    return displays !== undefined && displays.length > 0 &&
        !!displays[0].mirroringSourceId;
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} display
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  isSelected_: function(display, selectedDisplay) {
    return display.id == selectedDisplay.id;
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  enableSetResolution_: function(selectedDisplay) {
    return selectedDisplay.modes.length > 1;
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  enableDisplayZoomSlider_: function(selectedDisplay) {
    return selectedDisplay.availableDisplayZoomFactors.length > 1;
  },

  /**
   * Returns true if the given mode is the best mode for the |selectedDisplay|.
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @param {!chrome.system.display.DisplayMode} mode
   * @return {boolean}
   * @private
   */
  isBestMode_: function(selectedDisplay, mode) {
    if (!selectedDisplay.isInternal) {
      return mode.isNative;
    }

    // Things work differently for full HD devices(1080p). The best mode is the
    // one with 1.25 device scale factor and 0.8 ui scale.
    if (mode.heightInNativePixels == 1080) {
      return Math.abs(mode.uiScale - 0.8) < 0.001 &&
          Math.abs(mode.deviceScaleFactor - 1.25) < 0.001;
    }

    return mode.uiScale == 1.0;
  },

  /**
   * @return {string}
   * @private
   */
  getResolutionText_: function() {
    if (this.selectedDisplay.modes.length == 0 ||
        this.currentSelectedModeIndex_ == -1) {
      // If currentSelectedModeIndex_ == -1, selectedDisplay and
      // |selectedModePref_.value| are not in sync.
      return this.i18n(
          'displayResolutionText', this.selectedDisplay.bounds.width.toString(),
          this.selectedDisplay.bounds.height.toString());
    }
    const mode = this.selectedDisplay.modes[
        /** @type {number} */ (this.selectedModePref_.value)];
    assert(mode);
    const widthStr = mode.width.toString();
    const heightStr = mode.height.toString();
    if (this.isBestMode_(this.selectedDisplay, mode)) {
      return this.i18n('displayResolutionTextBest', widthStr, heightStr);
    } else if (mode.isNative) {
      return this.i18n('displayResolutionTextNative', widthStr, heightStr);
    }
    return this.i18n('displayResolutionText', widthStr, heightStr);
  },

  /**
   * Updates the logical resolution text to be used for the display size section
   * @param {number} zoomFactor Current zoom factor applied on the selected
   *    display.
   * @private
   */
  updateLogicalResolutionText_: function(zoomFactor) {
    if (!this.selectedDisplay.isInternal) {
      this.logicalResolutionText_ = '';
      return;
    }
    const mode = this.selectedDisplay.modes[this.currentSelectedModeIndex_];
    const deviceScaleFactor = mode.deviceScaleFactor;
    const inverseZoomFactor = 1.0 / zoomFactor;
    let logicalResolutionStrId = 'displayZoomLogicalResolutionText';
    if (Math.abs(deviceScaleFactor - inverseZoomFactor) < 0.001) {
      logicalResolutionStrId = 'displayZoomNativeLogicalResolutionNativeText';
    } else if (Math.abs(inverseZoomFactor - 1.0) < 0.001) {
      logicalResolutionStrId = 'displayZoomLogicalResolutionDefaultText';
    }
    let widthStr =
        Math.round(mode.widthInNativePixels / (deviceScaleFactor * zoomFactor))
            .toString();
    let heightStr =
        Math.round(mode.heightInNativePixels / (deviceScaleFactor * zoomFactor))
            .toString();
    if (this.shouldSwapLogicalResolutionText_()) {
      const temp = widthStr;
      widthStr = heightStr;
      heightStr = temp;
    }
    this.logicalResolutionText_ =
        this.i18n(logicalResolutionStrId, widthStr, heightStr);
  },

  /**
   * Determines whether width and height should be swapped in the
   * Logical Resolution Text. Returns true if the aspect ratio of the display's
   * native pixels is not equal to the aspect ratio of the displays current
   * bounds.
   * @private
   */
  shouldSwapLogicalResolutionText_: function() {
    const mode = this.selectedDisplay.modes[this.currentSelectedModeIndex_];
    const bounds = this.selectedDisplay.bounds;

    return (bounds.width / bounds.height).toPrecision(4) !=
        (mode.widthInNativePixels / mode.heightInNativePixels).toPrecision(4);
  },


  /**
   * Handles the event where the display size slider is being dragged, i.e. the
   * mouse or tap has not been released.
   * @private
   */
  onDisplaySizeSliderDrag_: function() {
    if (!this.selectedDisplay) {
      return;
    }

    const sliderValue = this.$.displaySizeSlider.$$('#slider').value;
    const zoomFactor = this.$.displaySizeSlider.ticks[sliderValue].value;
    this.updateLogicalResolutionText_(
        /** @type {number} */ (zoomFactor));
  },

  /**
   * @param {!CustomEvent<string>} e |e.detail| is the id of the selected
   *     display.
   * @private
   */
  onSelectDisplay_: function(e) {
    const id = e.detail;
    for (let i = 0; i < this.displays.length; ++i) {
      const display = this.displays[i];
      if (id == display.id) {
        if (this.selectedDisplay != display) {
          this.setSelectedDisplay_(display);
        }
        return;
      }
    }
  },

  /** @private */
  onSelectDisplayTab_: function() {
    const {selected} = this.$$('cr-tabs');
    if (this.selectedTab_ != selected) {
      this.setSelectedDisplay_(this.displays[selected]);
    }
  },

  /**
   * Handles event when a touch calibration option is selected.
   * @param {!Event} e
   * @private
   */
  onTouchCalibrationTap_: function(e) {
    settings.display.systemDisplayApi.showNativeTouchCalibration(
        this.selectedDisplay.id);
  },

  /**
   * Handles the event when an option from display select menu is selected.
   * @param {!{target: !HTMLSelectElement}} e
   * @private
   */
  updatePrimaryDisplay_: function(e) {
    /** @type {number} */ const PRIMARY_DISP_IDX = 0;
    if (!this.selectedDisplay) {
      return;
    }
    if (this.selectedDisplay.id == this.primaryDisplayId) {
      return;
    }
    if (e.target.value != PRIMARY_DISP_IDX) {
      return;
    }

    /** @type {!chrome.system.display.DisplayProperties} */ const properties = {
      isPrimary: true
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /**
   * Updates the selected mode based on the latest pref value.
   * @private
   */
  onSelectedModeSliderChange_: function() {
    if (this.currentSelectedModeIndex_ == -1 ||
        this.currentSelectedModeIndex_ == this.selectedModePref_.value) {
      // Don't change the selected display mode until we have received an update
      // from Chrome and the mode differs from the current mode.
      return;
    }
    /** @type {!chrome.system.display.DisplayProperties} */ const properties = {
      displayMode: this.selectedDisplay.modes[
          /** @type {number} */ (this.selectedModePref_.value)]
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /**
   * Handles a change in the |selectedModePref| value triggered via the observer
   * @param {number} newModeIndex The new index value for which thie function is
   *     called.
   * @private
   */
  onSelectedModeChange_: function(newModeIndex) {
    // We want to ignore all value changes to the pref due to the slider being
    // dragged. See http://crbug/845712 for more info.
    if (this.currentSelectedModeIndex_ == newModeIndex) {
      return;
    }
    this.onSelectedModeSliderChange_();
  },

  /**
   * Triggerend when the display size slider changes its value. This only
   * occurs when the value is committed (i.e. not while the slider is being
   * dragged).
   * @private
   */
  onSelectedZoomChange_: function() {
    if (this.currentSelectedModeIndex_ == -1 || !this.selectedDisplay) {
      return;
    }

    /** @type {!chrome.system.display.DisplayProperties} */ const properties = {
      displayZoomFactor:
          /** @type {number} */ (this.selectedZoomPref_.value)
    };

    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /**
   * Returns whether the option "Auto-rotate" is one of the shown options in the
   * rotation drop-down menu.
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean|undefined}
   * @private
   */
  showAutoRotateOption_: function(selectedDisplay) {
    return selectedDisplay.isInTabletPhysicalState;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onOrientationChange_: function(event) {
    const target = /** @type {!HTMLSelectElement} */ (event.target);
    const value = /** @type {number} */ (parseInt(target.value, 10));

    assert(value != -1 || this.selectedDisplay.isInTabletPhysicalState);

    /** @type {!chrome.system.display.DisplayProperties} */ const properties = {
      rotation: value
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.selectedDisplay.id, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /** @private */
  onMirroredTap_: function(event) {
    // Blur the control so that when the transition animation completes and the
    // UI is focused, the control does not receive focus. crbug.com/785070
    event.target.blur();

    /** @type {!chrome.system.display.MirrorModeInfo} */
    const mirrorModeInfo = {
      mode: this.isMirrored_(this.displays) ?
          chrome.system.display.MirrorMode.OFF :
          chrome.system.display.MirrorMode.NORMAL
    };
    settings.display.systemDisplayApi.setMirrorMode(mirrorModeInfo, () => {
      const error = chrome.runtime.lastError;
      if (error) {
        console.error('setMirrorMode Error: ' + error.message);
      }
    });
  },

  /** @private */
  onUnifiedDesktopTap_: function() {
    /** @type {!chrome.system.display.DisplayProperties} */ const properties = {
      isUnified: !this.unifiedDesktopMode_,
    };
    settings.display.systemDisplayApi.setDisplayProperties(
        this.primaryDisplayId, properties,
        this.setPropertiesCallback_.bind(this));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onOverscanTap_: function(e) {
    e.preventDefault();
    this.overscanDisplayId = this.selectedDisplay.id;
    this.showOverscanDialog_(true);
  },

  /** @private */
  onCloseOverscanDialog_: function() {
    cr.ui.focusWithoutInk(assert(this.$$('#overscan')));
  },

  /** @private */
  updateDisplayInfo_: function() {
    let displayIds = '';
    let primaryDisplay = undefined;
    let selectedDisplay = undefined;
    for (let i = 0; i < this.displays.length; ++i) {
      const display = this.displays[i];
      if (displayIds) {
        displayIds += ',';
      }
      displayIds += display.id;
      if (display.isPrimary && !primaryDisplay) {
        primaryDisplay = display;
      }
      if (this.selectedDisplay && display.id == this.selectedDisplay.id) {
        selectedDisplay = display;
      }
    }
    this.displayIds = displayIds;
    this.primaryDisplayId = (primaryDisplay && primaryDisplay.id) || '';
    selectedDisplay = selectedDisplay || primaryDisplay ||
        (this.displays && this.displays[0]);
    this.setSelectedDisplay_(selectedDisplay);

    this.unifiedDesktopMode_ = !!primaryDisplay && primaryDisplay.isUnified;
  },

  /** @private */
  setPropertiesCallback_: function() {
    if (chrome.runtime.lastError) {
      console.error(
          'setDisplayProperties Error: ' + chrome.runtime.lastError.message);
    }
  },

  /**
   * Invoked when the status of Night Light or its schedule type are changed, in
   * order to update the schedule settings, such as whether to show the custom
   * schedule slider, and the schedule sub label.
   * @private
   */
  updateNightLightScheduleSettings_: function() {
    const scheduleType = this.getPref('ash.night_light.schedule_type').value;
    this.shouldOpenCustomScheduleCollapse_ =
        scheduleType == NightLightScheduleType.CUSTOM;

    if (scheduleType == NightLightScheduleType.SUNSET_TO_SUNRISE) {
      const nightLightStatus = this.getPref('ash.night_light.enabled').value;
      this.nightLightScheduleSubLabel_ = nightLightStatus ?
          this.i18n('displayNightLightOffAtSunrise') :
          this.i18n('displayNightLightOnAtSunset');
    } else {
      this.nightLightScheduleSubLabel_ = '';
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowArrangementSection_: function() {
    return this.hasMultipleDisplays_() || this.isMirrored_(this.displays);
  },

  /** @private */
  onDisplaysChanged_: function() {
    Polymer.dom.flush();
    const displayLayout = this.$$('#displayLayout');
    if (displayLayout) {
      displayLayout.updateDisplays(
          this.displays, this.layouts, this.mirroringDestinationIds);
    }
  },
});
