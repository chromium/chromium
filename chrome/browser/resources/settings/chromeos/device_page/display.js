// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display' is the settings subpage for display settings.
 */
cr.define('settings.display', function() {
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

  Polymer({
    is: 'settings-display',

    behaviors: [
      DeepLinkingBehavior,
      I18nBehavior,
      PrefsBehavior,
      settings.RouteObserverBehavior,
    ],

    properties: {
      /**
       * @type {!chrome.settingsPrivate.PrefObject}
       * @private
       */
      selectedModePref_: {
        type: Object,
        value() {
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
        value() {
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
       * @private {!Array<cr_slider.SliderTick>} Display zoom slider tick
       *     values.
       */
      zoomValues_: Array,

      /** @private {!DropdownMenuOptionList} */
      displayModeList_: {
        type: Array,
        value: [],
      },

      /** @private {!DropdownMenuOptionList} */
      refreshRateList_: {
        type: Array,
        value: [],
      },

      /** @private */
      unifiedDesktopAvailable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('unifiedDesktopAvailable');
        }
      },

      /** @private */
      ambientColorAvailable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('deviceSupportsAmbientColor');
        }
      },

      /** @private */
      listAllDisplayModes_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('listAllDisplayModes');
        }
      },

      /** @private */
      unifiedDesktopMode_: {
        type: Boolean,
        value: false,
      },

      /**
       * @type {!chrome.settingsPrivate.PrefObject}
       * @private
       */
      selectedParentModePref_: {
        type: Object,
        value: function() {
          return {
            key: 'fakeDisplayParentModePref',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 0,
          };
        },
      },

      /** @private */
      scheduleTypesList_: {
        type: Array,
        value() {
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

      /**
       * Contains the settingId of any deep link that wasn't able to be shown,
       * null otherwise.
       * @private {?chromeos.settings.mojom.Setting}
       */
      pendingSettingId_: {
        type: Number,
        value: null,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!chromeos.settings.mojom.Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          chromeos.settings.mojom.Setting.kDisplaySize,
          chromeos.settings.mojom.Setting.kNightLight,
          chromeos.settings.mojom.Setting.kDisplayOrientation,
          chromeos.settings.mojom.Setting.kDisplayArrangement,
          chromeos.settings.mojom.Setting.kDisplayResolution,
          chromeos.settings.mojom.Setting.kDisplayRefreshRate,
          chromeos.settings.mojom.Setting.kDisplayMirroring,
          chromeos.settings.mojom.Setting.kAllowWindowsToSpanDisplays,
          chromeos.settings.mojom.Setting.kAmbientColors,
          chromeos.settings.mojom.Setting.kTouchscreenCalibration,
          chromeos.settings.mojom.Setting.kNightLightColorTemperature,
          chromeos.settings.mojom.Setting.kDisplayOverscan,
        ]),
      },
    },

    observers: [
      'updateNightLightScheduleSettings_(prefs.ash.night_light.schedule_type.*,' +
          ' prefs.ash.night_light.enabled.*)',
      'onSelectedModeChange_(selectedModePref_.value)',
      'onSelectedParentModeChange_(selectedParentModePref_.value)',
      'onSelectedZoomChange_(selectedZoomPref_.value)',
      'onDisplaysChanged_(displays.*)',
    ],

    /**
     * This represents the index of the mode with the highest refresh rate at
     * the current resolution.
     * @private {number}
     */
    currentSelectedParentModeIndex_: -1,

    /**
     * This is the index of the currently selected mode.
     * @private {number} Selected mode index received from chrome.
     */
    currentSelectedModeIndex_: -1,

    /**
     * Listener for chrome.system.display.onDisplayChanged events.
     * @type {function(void)|undefined}
     * @private
     */
    displayChangedListener_: undefined,

    /** @private {?settings.DevicePageBrowserProxy} */
    browserProxy_: null,

    /** @private {boolean} */
    allowDisplayIdentificationApi_:
        loadTimeData.getBoolean('allowDisplayIdentificationApi'),

    /** @private {string} */
    invalidDisplayId_: loadTimeData.getString('invalidDisplayId'),

    /** @private {!settings.Route|undefined} */
    currentRoute_: undefined,

    /**
     * Maps a parentModeIndex to the list of possible refresh rates.
     * All modes have a modeIndex corresponding to the index in the selected
     * display's mode list. Parent mode indexes represent the mode with the
     * highest refresh rate at a given resolution. There is 1 and only 1
     * parentModeIndex for each possible resolution .
     * @private {!Map<number, DropdownMenuOptionList>}
     */
    parentModeToRefreshRateMap_: new Map(),

    /**
     * Map containing an entry for each display mode mapping its modeIndex to
     * the corresponding parentModeIndex value.
     * @private {!Map<number, number>} Mode index values for slider.
     */
    modeToParentModeMap_: new Map(),

    /** @override */
    created() {
      if (this.allowDisplayIdentificationApi_) {
        this.browserProxy_ = settings.DevicePageBrowserProxyImpl.getInstance();
      }
    },

    /** @override */
    attached() {
      this.displayChangedListener_ =
          this.displayChangedListener_ || this.getDisplayInfo_.bind(this);
      settings.getDisplayApi().onDisplayChanged.addListener(
          this.displayChangedListener_);

      this.getDisplayInfo_();
      this.$.displaySizeSlider.updateValueInstantly = false;
    },

    /** @override */
    detached() {
      settings.getDisplayApi().onDisplayChanged.removeListener(
          assert(this.displayChangedListener_));

      this.currentSelectedModeIndex_ = -1;
      this.currentSelectedParentModeIndex_ = -1;
    },

    /**
     * Overridden from DeepLinkingBehavior.
     * @param {!chromeos.settings.mojom.Setting} settingId
     * @return {boolean}
     */
    beforeDeepLinkAttempt(settingId) {
      if (!this.displays) {
        // On initial page load, displays will not be loaded and deep link
        // attempt will fail. Suppress warnings by exiting early and try again
        // in updateDisplayInfo_.
        return false;
      }

      // Continue with deep link attempt.
      return true;
    },

    /**
     * @param {!settings.Route|undefined} opt_newRoute
     * @param {!settings.Route|undefined} opt_oldRoute
     */
    currentRouteChanged(opt_newRoute, opt_oldRoute) {
      if (!this.allowDisplayIdentificationApi_) {
        return;
      }

      this.currentRoute_ = opt_newRoute;

      // When navigating away from the page, deselect any selected display.
      if (opt_newRoute !== settings.routes.DISPLAY &&
          opt_oldRoute === settings.routes.DISPLAY) {
        this.browserProxy_.highlightDisplay(this.invalidDisplayId_);
        return;
      }

      // Does not apply to this page.
      if (opt_newRoute !== settings.routes.DISPLAY) {
        this.pendingSettingId_ = null;
        return;
      }

      this.attemptDeepLink().then(result => {
        if (!result.deepLinkShown && result.pendingSettingId) {
          // Store any deep link settingId that wasn't shown so we can try again
          // in updateDisplayInfo_.
          this.pendingSettingId_ = result.pendingSettingId;
        }
      });
    },

    /**
     * Shows or hides the overscan dialog.
     * @param {boolean} showOverscan
     * @private
     */
    showOverscanDialog_(showOverscan) {
      if (showOverscan) {
        this.$.displayOverscan.open();
        this.$.displayOverscan.focus();
      } else {
        this.$.displayOverscan.close();
      }
    },

    /** @private */
    onDisplayIdsChanged_() {
      // Close any overscan dialog (which will cancel any overscan operation)
      // if displayIds changes.
      this.showOverscanDialog_(false);
    },

    /** @private */
    getDisplayInfo_() {
      /** @type {chrome.system.display.GetInfoFlags} */ const flags = {
        singleUnified: true
      };
      settings.getDisplayApi().getInfo(
          flags, this.displayInfoFetched_.bind(this));
    },

    /**
     * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
     * @private
     */
    displayInfoFetched_(displays) {
      if (!displays.length) {
        return;
      }
      settings.getDisplayApi().getDisplayLayout(
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
    displayLayoutFetched_(displays, layouts) {
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
    getSelectedModeIndex_(selectedDisplay) {
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
    isDevicePolicyEnabled_(policyPref) {
      return policyPref !== undefined && policyPref.value !== null;
    },

    /**
     * Checks if display resolution is managed by device policy.
     * @param {DisplayResolutionPrefObject} resolutionPref
     * @return {boolean}
     * @private
     */
    isDisplayResolutionManagedByPolicy_(resolutionPref) {
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
    isDisplayResolutionMandatory_(resolutionPref) {
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
    isDisplayScaleManagedByPolicy_(selectedDisplay, resolutionPref) {
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
    isDisplayScaleMandatory_(selectedDisplay, resolutionPref) {
      return this.isDisplayScaleManagedByPolicy_(
                 selectedDisplay, resolutionPref) &&
          !resolutionPref.value.recommended;
    },


    /**
     * Parses the display modes for |selectedDisplay|. |displayModeList_| will
     * contain entries representing a combined resolution + refresh rate.
     * Only one parse*DisplayModes_ method must be called, depending on the
     * state of |listAllDisplayModes_|.
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @private
     */
    parseCompoundDisplayModes_(selectedDisplay) {
      assert(!this.listAllDisplayModes_);
      const optionList = [];
      for (let i = 0; i < selectedDisplay.modes.length; ++i) {
        const mode = selectedDisplay.modes[i];

        const id = 'displayResolutionMenuItem';
        const refreshRate = Math.round(mode.refreshRate * 100) / 100;
        const resolution = this.i18n(
            id, mode.width.toString(), mode.height.toString(),
            refreshRate.toString());

        optionList.push({
          name: resolution,
          value: i,
        });
      }
      this.displayModeList_ = optionList;
    },

    /**
     * Uses the modes of |selectedDisplay| to build a nested map of width =>
     * height => refreshRate => modeIndex. modeIndex is the index of the
     * resolution + refreshRate combination in |selectedDisplay|'s mode list.
     * This is used to traverse all possible display modes in ascending order.
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @return {!Map<number, Map<number, Map<number, number>>>}
     * @private
     */
    createModeMap_(selectedDisplay) {
      const modes = new Map();
      for (let i = 0; i < selectedDisplay.modes.length; ++i) {
        const mode = selectedDisplay.modes[i];
        if (!modes.has(mode.width)) {
          modes.set(mode.width, new Map());
        }

        if (!modes.get(mode.width).has(mode.height)) {
          modes.get(mode.width).set(mode.height, new Map());
        }

        // Prefer the first native mode we find, for consistency.
        if (modes.get(mode.width).get(mode.height).has(mode.refreshRate)) {
          const existingModeIndex =
              modes.get(mode.width).get(mode.height).get(mode.refreshRate);
          const existingMode = selectedDisplay.modes[existingModeIndex];
          if (existingMode.isNative || !mode.isNative) {
            continue;
          }
        }
        modes.get(mode.width).get(mode.height).set(mode.refreshRate, i);
      }
      return modes;
    },

    /**
     * Parses the display modes for |selectedDisplay|. |displayModeList_| will
     * contain entries representing only resolution options.
     * The 'parentMode' for a resolution is the highest refresh rate. This
     * method goes through the mode list for a given display creating data
     * structures so that given a resolution, the default refresh rate is
     * selected, and other possible refresh rates at that resolution are shown
     * in a dropdown. Only one parse*DisplayModes_ method must be called,
     * depending on the state of |listAllDisplayModes_|.
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @private
     */
    parseSplitDisplayModes_(selectedDisplay) {
      assert(this.listAllDisplayModes_);
      // Clear the mappings before recalculating.
      this.modeToParentModeMap_ = new Map();
      this.parentModeToRefreshRateMap_ = new Map();
      this.displayModeList_ = new Array();

      // Build the modes into a nested map of width => height => refresh rate.
      const modes = this.createModeMap_(selectedDisplay);

      // Traverse the modes ordered by width (asc), height (asc),
      // refresh rate (desc).
      const widthsArr = Array.from(modes.keys()).sort();
      for (let i = 0; i < widthsArr.length; i++) {
        const width = widthsArr[i];
        const heightsMap = modes.get(width);
        const heightArr = Array.from(heightsMap.keys());
        for (let j = 0; j < heightArr.length; j++) {
          // The highest/first refresh rate for each width/height pair
          // (resolution) is the default and therefore the "parent" mode.
          const height = heightArr[j];
          const refreshRates = heightsMap.get(height);
          const parentModeIndex = this.getParentModeIndex_(refreshRates);
          this.addResolution_(parentModeIndex, width, height);

          // For each of the refresh rates at a given resolution, add an entry
          // to |parentModeToRefreshRateMap_|. This allows us to retrieve a
          // list of all the possible refresh rates given a resolution's
          // parentModeIndex.
          const refreshRatesArr = Array.from(refreshRates.keys());
          for (let k = 0; k < refreshRatesArr.length; k++) {
            const rate = refreshRatesArr[k];
            const modeIndex = refreshRates.get(rate);
            const isInterlaced = selectedDisplay.modes[modeIndex].isInterlaced;

            this.addRefreshRate_(
                parentModeIndex, modeIndex, rate, isInterlaced);
          }
        }
      }

      // Construct mode->parentMode map so we can get parent modes later.
      for (let i = 0; i < selectedDisplay.modes.length; i++) {
        const mode = selectedDisplay.modes[i];
        const parentModeIndex =
            this.getParentModeIndex_(modes.get(mode.width).get(mode.height));
        this.modeToParentModeMap_.set(i, parentModeIndex);
      }
      assert(this.modeToParentModeMap_.size === selectedDisplay.modes.length);

      // Use the new sort order.
      this.sortResolutionList_();
    },

    /**
     * Picks the appropriate parent mode from a refresh rate -> mode index map.
     * Currently this chooses the mode with the highest refresh rate.
     * @param {Map<number,number>} refreshRates each possible refresh rate
     *   mapped to the corresponding mode index.
     * @private
     */
    getParentModeIndex_(refreshRates) {
      const maxRefreshRate = Math.max(...refreshRates.keys());
      return refreshRates.get(maxRefreshRate);
    },

    /**
     * Adds a an entry in |displayModeList_| for the resolution represented by
     * |width| and |height| and possible |refreshRates|.
     * @param {number} parentModeIndex
     * @param {number} width
     * @param {number} height
     * @private
     */
    addResolution_(parentModeIndex, width, height) {
      assert(this.listAllDisplayModes_);

      // Add an entry in the outer map for |parentModeIndex|. The inner
      // array (the value at |parentModeIndex|) will be populated with all
      // possible refresh rates for the given resolution.
      this.parentModeToRefreshRateMap_.set(parentModeIndex, new Array());

      const resolutionOption =
          this.i18n('displayResolutionOnlyMenuItem', width, height);

      // Only store one entry in the |resolutionList| per resolution,
      // mapping it to the parentModeIndex for that resolution.
      this.push('displayModeList_', {
        name: resolutionOption,
        value: parentModeIndex,
      });
    },

    /**
     * Adds a an entry in |parentModeToRefreshRateMap_| for the refresh rate
     * represented by |rate|.
     * @param {number} parentModeIndex
     * @param {number} modeIndex
     * @param {number} rate
     * @param {boolean|undefined} isInterlaced
     * @private
     */
    addRefreshRate_(parentModeIndex, modeIndex, rate, isInterlaced) {
      assert(this.listAllDisplayModes_);

      // Truncate at two decimal places for display. If the refresh rate
      // is a whole number, remove the mantissa.
      let refreshRate = Number(rate).toFixed(2);
      if (refreshRate.endsWith('.00')) {
        refreshRate = refreshRate.substring(0, refreshRate.length - 3);
      }

      const id = isInterlaced ? 'displayRefreshRateInterlacedMenuItem' :
                                'displayRefreshRateMenuItem';

      const refreshRateOption = this.i18n(id, refreshRate.toString());

      this.parentModeToRefreshRateMap_.get(parentModeIndex).push({
        name: refreshRateOption,
        value: modeIndex,
      });
    },

    /**
     * Sorts |displayModeList_| in descending order. First order sort is width,
     * second order sort is height.
     * @private
     */
    sortResolutionList_() {
      const getWidthFromResolutionString = function(str) {
        return Number(str.substr(0, str.indexOf(' ')));
      };

      this.displayModeList_ =
          this.displayModeList_
              .sort((first, second) => {
                return getWidthFromResolutionString(first.name) -
                    getWidthFromResolutionString(second.name);
              })
              .reverse();
    },

    /**
     * Parses display modes for |selectedDisplay|. A 'mode' is a resolution +
     * refresh rate combo. If |listAllDisplayModes_| is on, resolution and
     * refresh rate are parsed into separate dropdowns and
     * |parentModeToRefreshRateMap_| + |modeToParentModeMap_| are populated.
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @private
     */
    updateDisplayModeStructures_(selectedDisplay) {
      if (this.listAllDisplayModes_) {
        this.parseSplitDisplayModes_(selectedDisplay);
      } else {
        this.parseCompoundDisplayModes_(selectedDisplay);
      }
    },

    /**
     * Returns a value from |zoomValues_| that is closest to the display zoom
     * percentage currently selected for the |selectedDisplay|.
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @return {number}
     * @private
     */
    getSelectedDisplayZoom_(selectedDisplay) {
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
    getZoomValues_(selectedDisplay) {
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
    setSelectedDisplay_(selectedDisplay) {
      // |modeValues_| controls the resolution slider's tick values. Changing it
      // might trigger a change in the |selectedModePref_.value| if the number
      // of modes differs and the current mode index is out of range of the new
      // modes indices. Thus, we need to set |currentSelectedModeIndex_| to -1
      // to indicate that the |selectedDisplay| and |selectedModePref_.value|
      // are out of sync, and therefore getResolutionText_() and
      // onSelectedModeChange_() will be no-ops.
      this.currentSelectedModeIndex_ = -1;
      this.currentSelectedParentModeIndex_ = -1;
      const numModes = selectedDisplay.modes.length;
      this.modeValues_ =
          numModes === 0 ? [] : Array.from(Array(numModes).keys());

      // Note that the display zoom values has the same number of ticks for all
      // displays, so the above problem doesn't apply here.
      this.zoomValues_ = this.getZoomValues_(selectedDisplay);
      this.set(
          'selectedZoomPref_.value',
          this.getSelectedDisplayZoom_(selectedDisplay));

      this.updateDisplayModeStructures_(selectedDisplay);

      // Set |selectedDisplay| first since only the resolution slider depends
      // on |selectedModePref_|.
      this.selectedDisplay = selectedDisplay;
      this.selectedTab_ = this.displays.indexOf(this.selectedDisplay);

      const currentModeIndex = this.getSelectedModeIndex_(selectedDisplay);

      this.currentSelectedModeIndex_ = currentModeIndex;
      // This will also cause the parent mode to be updated.
      this.set('selectedModePref_.value', this.currentSelectedModeIndex_);

      if (this.listAllDisplayModes_) {
        // Now that everything is in sync, set the selected mode to its correct
        // value right before updating the pref.
        this.currentSelectedParentModeIndex_ =
            this.modeToParentModeMap_.get(currentModeIndex);
        this.refreshRateList_ = this.parentModeToRefreshRateMap_.get(
            this.currentSelectedParentModeIndex_);
      } else {
        this.currentSelectedParentModeIndex_ = currentModeIndex;
      }

      this.set(
          'selectedParentModePref_.value',
          this.currentSelectedParentModeIndex_);

      this.updateLogicalResolutionText_(
          /** @type {number} */ (this.selectedZoomPref_.value));
    },

    /**
     * Returns true if the resolution setting needs to be displayed.
     * @param {!chrome.system.display.DisplayUnitInfo} display
     * @return {boolean}
     * @private
     */
    showDropDownResolutionSetting_(display) {
      return !display.isInternal;
    },

    /**
     * Returns true if the refresh rate setting needs to be displayed.
     * @param {!chrome.system.display.DisplayUnitInfo} display
     * @return {boolean}
     * @private
     */
    showRefreshRateSetting_(display) {
      return this.listAllDisplayModes_ &&
          this.showDropDownResolutionSetting_(display);
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
    showTouchCalibrationSetting_(display) {
      return !display.isInternal &&
          loadTimeData.getBoolean('enableTouchCalibrationSetting');
    },

    /**
     * Returns true if the overscan setting should be shown for |display|.
     * @param {!chrome.system.display.DisplayUnitInfo} display
     * @return {boolean}
     * @private
     */
    showOverscanSetting_(display) {
      return !display.isInternal;
    },

    /**
     * Returns true if the ambient color setting should be shown for |display|.
     * @param {boolean} ambientColorAvailable
     * @param {chrome.system.display.DisplayUnitInfo} display
     * @return {boolean}
     * @private
     */
    showAmbientColorSetting_(ambientColorAvailable, display) {
      return ambientColorAvailable && display && display.isInternal;
    },

    /**
     * @return {boolean}
     * @private
     */
    hasMultipleDisplays_() {
      return this.displays.length > 1;
    },

    /**
     * Returns false if the display select menu has to be hidden.
     * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @return {boolean}
     * @private
     */
    showDisplaySelectMenu_(displays, selectedDisplay) {
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
    getDisplaySelectMenuIndex_(selectedDisplay, primaryDisplayId) {
      if (selectedDisplay && selectedDisplay.id === primaryDisplayId) {
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
    getDisplayMirrorText_(displays) {
      return this.i18n('displayMirror', displays[0].name);
    },

    /**
     * @param {boolean} unifiedDesktopAvailable
     * @param {boolean} unifiedDesktopMode
     * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
     * @return {boolean}
     * @private
     */
    showUnifiedDesktop_(unifiedDesktopAvailable, unifiedDesktopMode, displays) {
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
    getUnifiedDesktopText_(unifiedDesktopMode) {
      return this.i18n(
          unifiedDesktopMode ? 'displayUnifiedDesktopOn' :
                               'displayUnifiedDesktopOff');
    },

    /**
     * @param {boolean} unifiedDesktopMode
     * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
     * @return {boolean}
     * @private
     */
    showMirror_(unifiedDesktopMode, displays) {
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
    isMirrored_(displays) {
      return displays !== undefined && displays.length > 0 &&
          !!displays[0].mirroringSourceId;
    },

    /**
     * @param {!chrome.system.display.DisplayUnitInfo} display
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @return {boolean}
     * @private
     */
    isSelected_(display, selectedDisplay) {
      return display.id === selectedDisplay.id;
    },

    /**
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @return {boolean}
     * @private
     */
    enableSetResolution_(selectedDisplay) {
      return selectedDisplay.modes.length > 1;
    },

    /**
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @return {boolean}
     * @private
     */
    enableDisplayZoomSlider_(selectedDisplay) {
      return selectedDisplay.availableDisplayZoomFactors.length > 1;
    },

    /**
     * Returns true if the given mode is the best mode for the
     * |selectedDisplay|.
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @param {!chrome.system.display.DisplayMode} mode
     * @return {boolean}
     * @private
     */
    isBestMode_(selectedDisplay, mode) {
      if (!selectedDisplay.isInternal) {
        return mode.isNative;
      }

      // Things work differently for full HD devices(1080p). The best mode is
      // the one with 1.25 device scale factor and 0.8 ui scale.
      if (mode.heightInNativePixels === 1080) {
        return Math.abs(mode.uiScale - 0.8) < 0.001 &&
            Math.abs(mode.deviceScaleFactor - 1.25) < 0.001;
      }

      return mode.uiScale === 1.0;
    },

    /**
     * @return {string}
     * @private
     */
    getResolutionText_() {
      if (this.selectedDisplay.modes.length === 0 ||
          this.currentSelectedModeIndex_ === -1) {
        // If currentSelectedModeIndex_ == -1, selectedDisplay and
        // |selectedModePref_.value| are not in sync.
        return this.i18n(
            'displayResolutionText',
            this.selectedDisplay.bounds.width.toString(),
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
     * Updates the logical resolution text to be used for the display size
     * section
     * @param {number} zoomFactor Current zoom factor applied on the selected
     *    display.
     * @private
     */
    updateLogicalResolutionText_(zoomFactor) {
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
          Math.round(
                  mode.widthInNativePixels / (deviceScaleFactor * zoomFactor))
              .toString();
      let heightStr =
          Math.round(
                  mode.heightInNativePixels / (deviceScaleFactor * zoomFactor))
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
     * Logical Resolution Text. Returns true if the longer edge of the
     * display's native pixels is different than the longer edge of the
     * display's current bounds.
     * @private
     */
    shouldSwapLogicalResolutionText_() {
      const mode = this.selectedDisplay.modes[this.currentSelectedModeIndex_];
      const bounds = this.selectedDisplay.bounds;

      return bounds.width > bounds.height !==
          mode.widthInNativePixels > mode.heightInNativePixels;
    },


    /**
     * Handles the event where the display size slider is being dragged, i.e.
     * the mouse or tap has not been released.
     * @private
     */
    onDisplaySizeSliderDrag_() {
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
    onSelectDisplay_(e) {
      const id = e.detail;
      for (let i = 0; i < this.displays.length; ++i) {
        const display = this.displays[i];
        if (id === display.id) {
          if (this.selectedDisplay !== display) {
            this.setSelectedDisplay_(display);
          }
          return;
        }
      }
    },

    /** @private */
    onSelectDisplayTab_() {
      const {selected} = this.$$('cr-tabs');
      if (this.selectedTab_ !== selected) {
        this.setSelectedDisplay_(this.displays[selected]);
      }
    },

    /**
     * Handles event when a touch calibration option is selected.
     * @param {!Event} e
     * @private
     */
    onTouchCalibrationTap_(e) {
      settings.getDisplayApi().showNativeTouchCalibration(
          this.selectedDisplay.id);
    },

    /**
     * Handles the event when an option from display select menu is selected.
     * @param {!{target: !HTMLSelectElement}} e
     * @private
     */
    updatePrimaryDisplay_(e) {
      /** @type {number} */ const PRIMARY_DISP_IDX = 0;
      if (!this.selectedDisplay) {
        return;
      }
      if (this.selectedDisplay.id === this.primaryDisplayId) {
        return;
      }
      if (!e.target.value) {
        return;
      }

      /** @type {!chrome.system.display.DisplayProperties} */ const properties =
          {isPrimary: true};
      settings.getDisplayApi().setDisplayProperties(
          this.selectedDisplay.id, properties,
          this.setPropertiesCallback_.bind(this));
    },

    /**
     * Handles a change in the |selectedParentModePref| value triggered via the
     * observer.
     * @param {number} newModeIndex The new index value
     * @private
     */
    onSelectedParentModeChange_(newModeIndex) {
      if (this.currentSelectedParentModeIndex_ === newModeIndex) {
        return;
      }

      if (!this.hasNewParentModeBeenSet()) {
        // Don't change the selected display mode until we have received an
        // update from Chrome and the mode differs from the current mode.
        return;
      }

      // Reset |selectedModePref| to the parentMode.
      this.set('selectedModePref_.value', this.selectedParentModePref_.value);
    },

    /**
     * Returns True if a new parentMode has been set and we have received an
     * update from Chrome.
     * @return {boolean}
     * @private
     */
    hasNewParentModeBeenSet() {
      if (this.currentSelectedParentModeIndex_ === -1) {
        return false;
      }

      return this.currentSelectedParentModeIndex_ !==
          this.selectedParentModePref_.value;
    },

    /**
     * Returns True if a new mode has been set and we have received an update
     * from Chrome.
     * @return {boolean}
     * @private
     */
    hasNewModeBeenSet() {
      if (this.currentSelectedModeIndex_ === -1) {
        return false;
      }

      if (this.currentSelectedParentModeIndex_ !==
          this.selectedParentModePref_.value) {
        return true;
      }

      return this.currentSelectedModeIndex_ !== this.selectedModePref_.value;
    },

    /**
     * Handles a change in |selectedModePref| triggered via the observer.
     * @param {number} newModeIndex The new index value
     * @private
     */
    onSelectedModeChange_(newModeIndex) {
      // We want to ignore all value changes to the pref due to the slider being
      // dragged. See http://crbug/845712 for more info.
      if (this.currentSelectedModeIndex_ === newModeIndex) {
        return;
      }

      if (!this.hasNewModeBeenSet()) {
        // Don't change the selected display mode until we have received an
        // update from Chrome and the mode differs from the current mode.
        return;
      }
      /** @type {!chrome.system.display.DisplayProperties} */ const properties =
          {
            displayMode: this.selectedDisplay.modes[
                /** @type {number} */ (this.selectedModePref_.value)]
          };

      this.refreshRateList_ = this.parentModeToRefreshRateMap_.get(
          /** @type {number} */ (this.selectedParentModePref_.value));
      settings.getDisplayApi().setDisplayProperties(
          this.selectedDisplay.id, properties,
          this.setPropertiesCallback_.bind(this));
    },

    /**
     * Triggerend when the display size slider changes its value. This only
     * occurs when the value is committed (i.e. not while the slider is being
     * dragged).
     * @private
     */
    onSelectedZoomChange_() {
      if (this.currentSelectedModeIndex_ === -1 || !this.selectedDisplay) {
        return;
      }

      /** @type {!chrome.system.display.DisplayProperties} */ const properties =
          {
            displayZoomFactor:
                /** @type {number} */ (this.selectedZoomPref_.value)
          };

      settings.getDisplayApi().setDisplayProperties(
          this.selectedDisplay.id, properties,
          this.setPropertiesCallback_.bind(this));
    },

    /**
     * Returns whether the option "Auto-rotate" is one of the shown options in
     * the rotation drop-down menu.
     * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
     * @return {boolean|undefined}
     * @private
     */
    showAutoRotateOption_(selectedDisplay) {
      return selectedDisplay.isAutoRotationAllowed;
    },

    /**
     * @param {!Event} event
     * @private
     */
    onOrientationChange_(event) {
      const target = /** @type {!HTMLSelectElement} */ (event.target);
      const value = /** @type {number} */ (parseInt(target.value, 10));

      assert(value !== -1 || this.selectedDisplay.isAutoRotationAllowed);

      /** @type {!chrome.system.display.DisplayProperties} */ const properties =
          {rotation: value};
      settings.getDisplayApi().setDisplayProperties(
          this.selectedDisplay.id, properties,
          this.setPropertiesCallback_.bind(this));
    },

    /** @private */
    onMirroredTap_(event) {
      // Blur the control so that when the transition animation completes and
      // the UI is focused, the control does not receive focus. crbug.com/785070
      event.target.blur();

      /** @type {!chrome.system.display.MirrorModeInfo} */
      const mirrorModeInfo = {
        mode: this.isMirrored_(this.displays) ?
            chrome.system.display.MirrorMode.OFF :
            chrome.system.display.MirrorMode.NORMAL
      };
      settings.getDisplayApi().setMirrorMode(mirrorModeInfo, () => {
        const error = chrome.runtime.lastError;
        if (error) {
          console.error('setMirrorMode Error: ' + error.message);
        }
      });
    },

    /** @private */
    onUnifiedDesktopTap_() {
      /** @type {!chrome.system.display.DisplayProperties} */ const properties =
          {
            isUnified: !this.unifiedDesktopMode_,
          };
      settings.getDisplayApi().setDisplayProperties(
          this.primaryDisplayId, properties,
          this.setPropertiesCallback_.bind(this));
    },

    /**
     * @param {!Event} e
     * @private
     */
    onOverscanTap_(e) {
      e.preventDefault();
      this.overscanDisplayId = this.selectedDisplay.id;
      this.showOverscanDialog_(true);
    },

    /** @private */
    onCloseOverscanDialog_() {
      cr.ui.focusWithoutInk(assert(this.$$('#overscan')));
    },

    /** @private */
    updateDisplayInfo_() {
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
        if (this.selectedDisplay && display.id === this.selectedDisplay.id) {
          selectedDisplay = display;
        }
      }
      this.displayIds = displayIds;
      this.primaryDisplayId = (primaryDisplay && primaryDisplay.id) || '';
      selectedDisplay = selectedDisplay || primaryDisplay ||
          (this.displays && this.displays[0]);
      this.setSelectedDisplay_(selectedDisplay);

      this.unifiedDesktopMode_ = !!primaryDisplay && primaryDisplay.isUnified;

      // Check if we have yet to focus a deep-linked element.
      if (!this.pendingSettingId_) {
        return;
      }

      this.showDeepLink(this.pendingSettingId_).then(result => {
        if (result.deepLinkShown) {
          this.pendingSettingId_ = null;
        }
      });
    },

    /** @private */
    setPropertiesCallback_() {
      if (chrome.runtime.lastError) {
        console.error(
            'setDisplayProperties Error: ' + chrome.runtime.lastError.message);
      }
    },

    /**
     * Invoked when the status of Night Light or its schedule type are changed,
     * in order to update the schedule settings, such as whether to show the
     * custom schedule slider, and the schedule sub label.
     * @private
     */
    updateNightLightScheduleSettings_() {
      const scheduleType = this.getPref('ash.night_light.schedule_type').value;
      this.shouldOpenCustomScheduleCollapse_ =
          scheduleType === NightLightScheduleType.CUSTOM;

      if (scheduleType === NightLightScheduleType.SUNSET_TO_SUNRISE) {
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
    shouldShowArrangementSection_() {
      if (!this.displays) {
        return false;
      }
      return this.hasMultipleDisplays_() || this.isMirrored_(this.displays);
    },

    /** @private */
    onDisplaysChanged_() {
      Polymer.dom.flush();
      const displayLayout = this.$$('#displayLayout');
      if (displayLayout) {
        displayLayout.updateDisplays(
            this.displays, this.layouts, this.mirroringDestinationIds);
      }
    },
  });

  // #cr_define_end
  return {};
});
