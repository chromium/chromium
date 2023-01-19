// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display' is the settings subpage for display settings.
 */

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './display_layout.js';
import './display_overscan_dialog.js';
import '../settings_scheduler_slider/settings_scheduler_slider.js';
import '../../controls/settings_slider.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';
import '../../controls/settings_dropdown_menu.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrSliderElement, SliderTick} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DropdownMenuOptionList} from '../../controls/settings_dropdown_menu.js';
import {SettingsSliderElement} from '../../controls/settings_slider.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {assertExists, cast, castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {routes} from '../os_route.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route} from '../router.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl, getDisplayApi} from './device_page_browser_proxy.js';
import {getTemplate} from './display.html.js';
import {SettingsDisplayOverscanDialogElement} from './display_overscan_dialog.js';

import DisplayLayout = chrome.system.display.DisplayLayout;
import DisplayMode = chrome.system.display.DisplayMode;
import DisplayProperties = chrome.system.display.DisplayProperties;
import DisplayUnitInfo = chrome.system.display.DisplayUnitInfo;
import GetInfoFlags = chrome.system.display.GetInfoFlags;
import MirrorMode = chrome.system.display.MirrorMode;
import MirrorModeInfo = chrome.system.display.MirrorModeInfo;

interface DisplayResolutionPrefObject {
  value: {
    recommended?: boolean,
    external_width?: number,
    external_height?: number,
    external_use_native?: boolean,
    external_scale_percentage?: number,
    internal_scale_percentage?: number,
  }|null;
}

/**
 * The types of Night Light automatic schedule. The values of the enum values
 * are synced with the pref "prefs.ash.night_light.schedule_type".
 */
enum NightLightScheduleType {
  NEVER = 0,
  SUNSET_TO_SUNRISE = 1,
  CUSTOM = 2,
}

interface SettingsDisplayElement {
  $: {
    displayOverscan: SettingsDisplayOverscanDialogElement,
    displaySizeSlider: SettingsSliderElement,
  };
}

const SettingsDisplayElementBase =
    DeepLinkingMixin(PrefsMixin(RouteObserverMixin(I18nMixin(PolymerElement))));

class SettingsDisplayElement extends SettingsDisplayElementBase {
  static get is() {
    return 'settings-display';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

      displays: Array,

      layouts: Array,

      /**
       * String listing the ids in displays. Used to observe changes to the
       * display configuration (i.e. when a display is added or removed).
       */
      displayIds: {type: String, observer: 'onDisplayIdsChanged_'},

      /** Primary display id */
      primaryDisplayId: String,

      selectedDisplay: Object,

      /** Id passed to the overscan dialog. */
      overscanDisplayId: {
        type: String,
        notify: true,
      },

      /** Ids for mirroring destination displays. */
      mirroringDestinationIds: Array,

      /** Mode index values for slider. */
      modeValues_: Array,

      /**
       * Display zoom slider tick values.
       */
      zoomValues_: Array,

      displayModeList_: {
        type: Array,
        value: [],
      },

      refreshRateList_: {
        type: Array,
        value: [],
      },

      unifiedDesktopAvailable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('unifiedDesktopAvailable');
        },
      },

      ambientColorAvailable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('deviceSupportsAmbientColor');
        },
      },

      listAllDisplayModes_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('listAllDisplayModes');
        },
      },

      unifiedDesktopMode_: {
        type: Boolean,
        value: false,
      },

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

      scheduleTypesList_: {
        type: Array,
        value() {
          return [
            {
              name: loadTimeData.getString('displayNightLightScheduleNever'),
              value: NightLightScheduleType.NEVER,
            },
            {
              name: loadTimeData.getString(
                  'displayNightLightScheduleSunsetToSunRise'),
              value: NightLightScheduleType.SUNSET_TO_SUNRISE,
            },
            {
              name: loadTimeData.getString('displayNightLightScheduleCustom'),
              value: NightLightScheduleType.CUSTOM,
            },
          ];
        },
      },

      shouldOpenCustomScheduleCollapse_: {
        type: Boolean,
        value: false,
      },

      nightLightScheduleSubLabel_: String,

      logicalResolutionText_: String,

      displayTabNames_: Array,

      selectedTab_: Number,

      /**
       * Contains the settingId of any deep link that wasn't able to be shown,
       * null otherwise.
       */
      pendingSettingId_: {
        type: Number,
        value: null,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kDisplaySize,
          Setting.kNightLight,
          Setting.kDisplayOrientation,
          Setting.kDisplayArrangement,
          Setting.kDisplayResolution,
          Setting.kDisplayRefreshRate,
          Setting.kDisplayMirroring,
          Setting.kAllowWindowsToSpanDisplays,
          Setting.kAmbientColors,
          Setting.kTouchscreenCalibration,
          Setting.kNightLightColorTemperature,
          Setting.kDisplayOverscan,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'updateNightLightScheduleSettings_(prefs.ash.night_light.schedule_type.*,' +
          ' prefs.ash.night_light.enabled.*)',
      'onSelectedModeChange_(selectedModePref_.value)',
      'onSelectedParentModeChange_(selectedParentModePref_.value)',
      'onSelectedZoomChange_(selectedZoomPref_.value)',
      'onDisplaysChanged_(displays.*)',

    ];
  }

  displayIds: string;
  displays: DisplayUnitInfo[];
  layouts: DisplayLayout[];
  mirroringDestinationIds: string[];
  overscanDisplayId: string;
  primaryDisplayId: string;
  selectedDisplay?: DisplayUnitInfo;
  private browserProxy_: DevicePageBrowserProxy;
  private currentRoute_: Route|null;
  private currentSelectedModeIndex_: number;
  private currentSelectedParentModeIndex_: number;
  private displayChangedListener_: (() => void)|null;
  private displayModeList_: DropdownMenuOptionList;
  private displayTabNames_: string[];
  private invalidDisplayId_: string;
  private listAllDisplayModes_: boolean;
  private logicalResolutionText_: string;
  private modeToParentModeMap_: Map<number, number>;
  private modeValues_: number[];
  private nightLightScheduleSubLabel_: string;
  private parentModeToRefreshRateMap_: Map<number, DropdownMenuOptionList>;
  private pendingSettingId_: Setting|null;
  private refreshRateList_: DropdownMenuOptionList;
  private selectedModePref_: chrome.settingsPrivate.PrefObject;
  private selectedParentModePref_: chrome.settingsPrivate.PrefObject;
  private selectedTab_: number;
  private selectedZoomPref_: chrome.settingsPrivate.PrefObject;
  private shouldOpenCustomScheduleCollapse_: boolean;
  private unifiedDesktopMode_: boolean;
  private zoomValues_: SliderTick[];

  constructor() {
    super();

    /**
     * This represents the index of the mode with the highest refresh rate at
     * the current resolution.
     */
    this.currentSelectedParentModeIndex_ = -1;

    /**
     * This is the index of the currently selected mode.
     * Selected mode index received from chrome.
     */
    this.currentSelectedModeIndex_ = -1;

    /**
     * Listener for chrome.system.display.onDisplayChanged events.
     */
    this.displayChangedListener_ = null;

    this.invalidDisplayId_ = loadTimeData.getString('invalidDisplayId');

    this.currentRoute_ = null;

    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();

    /**
     * Maps a parentModeIndex to the list of possible refresh rates.
     * All modes have a modeIndex corresponding to the index in the selected
     * display's mode list. Parent mode indexes represent the mode with the
     * highest refresh rate at a given resolution. There is 1 and only 1
     * parentModeIndex for each possible resolution .
     */
    this.parentModeToRefreshRateMap_ = new Map();

    /**
     * Map containing an entry for each display mode mapping its modeIndex to
     * the corresponding parentModeIndex value.
     * Mode index values for slider.
     */
    this.modeToParentModeMap_ = new Map();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.displayChangedListener_ =
        this.displayChangedListener_ || this.getDisplayInfo_.bind(this);
    getDisplayApi().onDisplayChanged.addListener(this.displayChangedListener_);

    this.getDisplayInfo_();
    this.$.displaySizeSlider.updateValueInstantly = false;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    getDisplayApi().onDisplayChanged.removeListener(
        castExists(this.displayChangedListener_));

    this.currentSelectedModeIndex_ = -1;
    this.currentSelectedParentModeIndex_ = -1;
  }

  override beforeDeepLinkAttempt(_settingId: Setting): boolean {
    if (!this.displays) {
      // On initial page load, displays will not be loaded and deep link
      // attempt will fail. Suppress warnings by exiting early and try again
      // in updateDisplayInfo_.
      return false;
    }

    // Continue with deep link attempt.
    return true;
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    this.currentRoute_ = newRoute;

    // When navigating away from the page, deselect any selected display.
    if (newRoute !== routes.DISPLAY && oldRoute === routes.DISPLAY) {
      this.browserProxy_.highlightDisplay(this.invalidDisplayId_);
      return;
    }

    // Does not apply to this page.
    if (newRoute !== routes.DISPLAY) {
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
  }

  /**
   * Shows or hides the overscan dialog.
   */
  private showOverscanDialog_(showOverscan: boolean) {
    if (showOverscan) {
      this.$.displayOverscan.open();
      this.$.displayOverscan.focus();
    } else {
      this.$.displayOverscan.close();
    }
  }

  private onDisplayIdsChanged_() {
    // Close any overscan dialog (which will cancel any overscan operation)
    // if displayIds changes.
    this.showOverscanDialog_(false);
  }

  private getDisplayInfo_() {
    const flags: GetInfoFlags = {
      singleUnified: true,
    };
    getDisplayApi().getInfo(flags).then(
        (displays: DisplayUnitInfo[]) => this.displayInfoFetched_(displays));
  }

  private displayInfoFetched_(displays: DisplayUnitInfo[]) {
    if (!displays.length) {
      return;
    }
    getDisplayApi().getDisplayLayout().then(
        (layouts: DisplayLayout[]) =>
            this.displayLayoutFetched_(displays, layouts));
    if (this.isMirrored_(displays)) {
      this.mirroringDestinationIds = displays[0].mirroringDestinationIds;
    } else {
      this.mirroringDestinationIds = [];
    }
  }

  private displayLayoutFetched_(
      displays: DisplayUnitInfo[], layouts: DisplayLayout[]) {
    this.layouts = layouts;
    this.displays = displays;
    this.displayTabNames_ = displays.map(({name}) => name);
    this.updateDisplayInfo_();
  }

  /**
   * @return The index of the currently selected mode of the
   * |selectedDisplay|. If the display has no modes, returns 0.
   */
  private getSelectedModeIndex_(selectedDisplay: DisplayUnitInfo): number {
    for (let i = 0; i < selectedDisplay.modes.length; ++i) {
      if (selectedDisplay.modes[i].isSelected) {
        return i;
      }
    }
    return 0;
  }

  private isDevicePolicyEnabled_(policyPref: DisplayResolutionPrefObject):
      boolean {
    return policyPref !== undefined && policyPref.value !== null;
  }

  private isDisplayResolutionManagedByPolicy_(
      resolutionPref: DisplayResolutionPrefObject): boolean {
    return this.isDevicePolicyEnabled_(resolutionPref) &&
        (resolutionPref.value!.external_use_native !== undefined ||
         (resolutionPref.value!.external_width !== undefined &&
          resolutionPref.value!.external_height !== undefined));
  }

  /**
   * Checks if display resolution is managed by policy and the policy
   * is mandatory.
   */
  private isDisplayResolutionMandatory_(
      resolutionPref: DisplayResolutionPrefObject): boolean {
    return this.isDisplayResolutionManagedByPolicy_(resolutionPref) &&
        !resolutionPref.value!.recommended;
  }

  /**
   * Checks if display scale factor is managed by device policy.
   */
  private isDisplayScaleManagedByPolicy_(
      selectedDisplay: DisplayUnitInfo,
      resolutionPref: DisplayResolutionPrefObject): boolean {
    if (!this.isDevicePolicyEnabled_(resolutionPref) || !selectedDisplay) {
      return false;
    }
    if (selectedDisplay.isInternal) {
      return resolutionPref.value!.internal_scale_percentage !== undefined;
    }
    return resolutionPref.value!.external_scale_percentage !== undefined;
  }

  /**
   * Checks if display scale factor is managed by policy and the policy
   * is mandatory.
   */
  private isDisplayScaleMandatory_(
      selectedDisplay: DisplayUnitInfo,
      resolutionPref: DisplayResolutionPrefObject): boolean {
    return this.isDisplayScaleManagedByPolicy_(
               selectedDisplay, resolutionPref) &&
        !resolutionPref.value!.recommended;
  }


  /**
   * Parses the display modes for |selectedDisplay|. |displayModeList_| will
   * contain entries representing a combined resolution + refresh rate.
   * Only one parse*DisplayModes_ method must be called, depending on the
   * state of |listAllDisplayModes_|.
   */
  private parseCompoundDisplayModes_(selectedDisplay: DisplayUnitInfo) {
    assert(!this.listAllDisplayModes_);
    const optionList: DropdownMenuOptionList = [];
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
  }

  /**
   * Uses the modes of |selectedDisplay| to build a nested map of width =>
   * height => refreshRate => modeIndex. modeIndex is the index of the
   * resolution + refreshRate combination in |selectedDisplay|'s mode list.
   * This is used to traverse all possible display modes in ascending order.
   */
  private createModeMap_(selectedDisplay: DisplayUnitInfo):
      Map<number, Map<number, Map<number, number>>> {
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
  }

  /**
   * Parses the display modes for |selectedDisplay|. |displayModeList_| will
   * contain entries representing only resolution options.
   * The 'parentMode' for a resolution is the highest refresh rate. This
   * method goes through the mode list for a given display creating data
   * structures so that given a resolution, the default refresh rate is
   * selected, and other possible refresh rates at that resolution are shown
   * in a dropdown. Only one parse*DisplayModes_ method must be called,
   * depending on the state of |listAllDisplayModes_|.
   */
  private parseSplitDisplayModes_(selectedDisplay: DisplayUnitInfo) {
    assert(this.listAllDisplayModes_);
    // Clear the mappings before recalculating.
    this.modeToParentModeMap_ = new Map();
    this.parentModeToRefreshRateMap_ = new Map();
    this.displayModeList_ = [];

    // Build the modes into a nested map of width => height => refresh rate.
    const modes = this.createModeMap_(selectedDisplay);

    // Traverse the modes ordered by width (asc), height (asc),
    // refresh rate (desc).
    const widthsArr = Array.from(modes.keys()).sort();
    for (let i = 0; i < widthsArr.length; i++) {
      const width = widthsArr[i];
      const heightsMap = modes.get(width)!;
      const heightArr = Array.from(heightsMap.keys());
      for (let j = 0; j < heightArr.length; j++) {
        // The highest/first refresh rate for each width/height pair
        // (resolution) is the default and therefore the "parent" mode.
        const height = heightArr[j];
        const refreshRates = heightsMap.get(height)!;
        const parentModeIndex = this.getParentModeIndex_(refreshRates);
        this.addResolution_(parentModeIndex, width, height);

        // For each of the refresh rates at a given resolution, add an entry
        // to |parentModeToRefreshRateMap_|. This allows us to retrieve a
        // list of all the possible refresh rates given a resolution's
        // parentModeIndex.
        const refreshRatesArr = Array.from(refreshRates.keys());
        for (let k = 0; k < refreshRatesArr.length; k++) {
          const rate = refreshRatesArr[k];
          const modeIndex = refreshRates.get(rate)!;
          const isInterlaced = selectedDisplay.modes[modeIndex].isInterlaced;

          this.addRefreshRate_(parentModeIndex, modeIndex, rate, isInterlaced);
        }
      }
    }

    // Construct mode->parentMode map so we can get parent modes later.
    for (let i = 0; i < selectedDisplay.modes.length; i++) {
      const mode = selectedDisplay.modes[i];
      const parentModeIndex =
          this.getParentModeIndex_(modes.get(mode.width)!.get(mode.height)!);
      this.modeToParentModeMap_.set(i, parentModeIndex);
    }
    assert(this.modeToParentModeMap_.size === selectedDisplay.modes.length);

    // Use the new sort order.
    this.sortResolutionList_();
  }

  /**
   * Picks the appropriate parent mode from a refresh rate -> mode index map.
   * Currently this chooses the mode with the highest refresh rate.
   * @param refreshRates each possible refresh rate
   *   mapped to the corresponding mode index.
   */
  private getParentModeIndex_(refreshRates: Map<number, number>) {
    const maxRefreshRate = Math.max(...refreshRates.keys());
    // maxRefreshRate always exists as a key
    return refreshRates.get(maxRefreshRate)!;
  }

  /**
   * Adds a an entry in |displayModeList_| for the resolution represented by
   * |width| and |height| and possible |refreshRates|.
   */
  private addResolution_(
      parentModeIndex: number, width: number, height: number) {
    assert(this.listAllDisplayModes_);

    // Add an entry in the outer map for |parentModeIndex|. The inner
    // array (the value at |parentModeIndex|) will be populated with all
    // possible refresh rates for the given resolution.
    this.parentModeToRefreshRateMap_.set(parentModeIndex, []);

    const resolutionOption =
        this.i18n('displayResolutionOnlyMenuItem', width, height);

    // Only store one entry in the |resolutionList| per resolution,
    // mapping it to the parentModeIndex for that resolution.
    this.push('displayModeList_', {
      name: resolutionOption,
      value: parentModeIndex,
    });
  }

  /**
   * Adds a an entry in |parentModeToRefreshRateMap_| for the refresh rate
   * represented by |rate|.
   */
  private addRefreshRate_(
      parentModeIndex: number, modeIndex: number, rate: number,
      isInterlaced?: boolean) {
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

    this.parentModeToRefreshRateMap_.get(parentModeIndex)!.push({
      name: refreshRateOption,
      value: modeIndex,
    });
  }

  /**
   * Sorts |displayModeList_| in descending order. First order sort is width,
   * second order sort is height.
   */
  private sortResolutionList_() {
    const getWidthFromResolutionString = function(str: string) {
      return Number(str.substr(0, str.indexOf(' ')));
    };

    this.displayModeList_ =
        this.displayModeList_
            .sort((first, second) => {
              return getWidthFromResolutionString(first.name) -
                  getWidthFromResolutionString(second.name);
            })
            .reverse();
  }

  /**
   * Parses display modes for |selectedDisplay|. A 'mode' is a resolution +
   * refresh rate combo. If |listAllDisplayModes_| is on, resolution and
   * refresh rate are parsed into separate dropdowns and
   * |parentModeToRefreshRateMap_| + |modeToParentModeMap_| are populated.
   */
  private updateDisplayModeStructures_(selectedDisplay: DisplayUnitInfo) {
    if (this.listAllDisplayModes_) {
      this.parseSplitDisplayModes_(selectedDisplay);
    } else {
      this.parseCompoundDisplayModes_(selectedDisplay);
    }
  }

  /**
   * Returns a value from |zoomValues_| that is closest to the display zoom
   * percentage currently selected for the |selectedDisplay|.
   */
  private getSelectedDisplayZoom_(selectedDisplay: DisplayUnitInfo): number {
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

    return closestMatch;
  }

  /**
   * Given the display with the current display mode, this function lists all
   * the display zoom values and their labels to be used by the slider.
   */
  private getZoomValues_(selectedDisplay: DisplayUnitInfo): SliderTick[] {
    return selectedDisplay.availableDisplayZoomFactors.map(value => {
      const ariaValue = Math.round(value * 100);
      return {
        value,
        ariaValue,
        label: this.i18n('displayZoomValue', ariaValue.toString()),
      };
    });
  }

  /**
   * We need to call this explicitly rather than relying on change events
   * so that we can control the update order.
   */
  private setSelectedDisplay_(selectedDisplay: DisplayUnitInfo) {
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
    this.modeValues_ = numModes === 0 ? [] : Array.from(Array(numModes).keys());

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
          this.modeToParentModeMap_.get(currentModeIndex)!;
      this.refreshRateList_ = this.parentModeToRefreshRateMap_.get(
          this.currentSelectedParentModeIndex_)!;
    } else {
      this.currentSelectedParentModeIndex_ = currentModeIndex;
    }

    this.set(
        'selectedParentModePref_.value', this.currentSelectedParentModeIndex_);

    this.updateLogicalResolutionText_(this.selectedZoomPref_.value);
  }

  /**
   * Returns true if the resolution setting needs to be displayed.
   */
  private showDropDownResolutionSetting_(display: DisplayUnitInfo): boolean {
    return !display.isInternal;
  }

  /**
   * Returns true if the refresh rate setting needs to be displayed.
   */
  private showRefreshRateSetting_(display: DisplayUnitInfo): boolean {
    return this.listAllDisplayModes_ &&
        this.showDropDownResolutionSetting_(display);
  }

  /**
   * Returns true if external touch devices are connected and the current
   * display is not an internal display. If the feature is not enabled via the
   * switch, this will return false.
   * @param display Display being checked for touch support.
   */
  private showTouchCalibrationSetting_(display: DisplayUnitInfo): boolean {
    return !display.isInternal &&
        loadTimeData.getBoolean('enableTouchCalibrationSetting');
  }

  /**
   * Returns true if the overscan setting should be shown for |display|.
   */
  private showOverscanSetting_(display: DisplayUnitInfo): boolean {
    return !display.isInternal;
  }

  /**
   * Returns true if the ambient color setting should be shown for |display|.
   */
  private showAmbientColorSetting_(
      ambientColorAvailable: boolean, display: DisplayUnitInfo): boolean {
    return ambientColorAvailable && display && display.isInternal;
  }

  private hasMultipleDisplays_(): boolean {
    return this.displays.length > 1;
  }

  /**
   * Returns false if the display select menu has to be hidden.
   */
  private showDisplaySelectMenu_(
      displays: DisplayUnitInfo[], selectedDisplay: DisplayUnitInfo): boolean {
    if (selectedDisplay) {
      return displays.length > 1 && !selectedDisplay.isPrimary;
    }

    return false;
  }

  /**
   * Returns the select menu index indicating whether the display currently is
   * primary or extended.
   * @return Returns 0 if the display is primary else returns 1.
   */
  private getDisplaySelectMenuIndex_(
      selectedDisplay: DisplayUnitInfo, primaryDisplayId: string): number {
    if (selectedDisplay && selectedDisplay.id === primaryDisplayId) {
      return 0;
    }
    return 1;
  }

  /**
   * Returns the i18n string for the text to be used for mirroring settings.
   * @return i18n string for mirroring settings text.
   */
  private getDisplayMirrorText_(displays: DisplayUnitInfo[]): string {
    return this.i18n('displayMirror', displays[0].name);
  }

  private showUnifiedDesktop_(
      unifiedDesktopAvailable: boolean, unifiedDesktopMode: boolean,
      displays: DisplayUnitInfo[]): boolean {
    if (displays === undefined) {
      return false;
    }

    return unifiedDesktopMode ||
        (unifiedDesktopAvailable && displays.length > 1 &&
         !this.isMirrored_(displays));
  }

  private getUnifiedDesktopText_(unifiedDesktopMode: boolean): string {
    return this.i18n(
        unifiedDesktopMode ? 'displayUnifiedDesktopOn' :
                             'displayUnifiedDesktopOff');
  }

  private showMirror_(unifiedDesktopMode: boolean, displays: DisplayUnitInfo[]):
      boolean {
    if (displays === undefined) {
      return false;
    }

    return this.isMirrored_(displays) ||
        (!unifiedDesktopMode && displays.length > 1);
  }

  private isMirrored_(displays: DisplayUnitInfo[]): boolean {
    return displays !== undefined && displays.length > 0 &&
        !!displays[0].mirroringSourceId;
  }

  private isSelected_(
      display: DisplayUnitInfo, selectedDisplay: DisplayUnitInfo): boolean {
    return display.id === selectedDisplay.id;
  }

  private enableSetResolution_(selectedDisplay: DisplayUnitInfo): boolean {
    return selectedDisplay.modes.length > 1;
  }

  private enableDisplayZoomSlider_(selectedDisplay: DisplayUnitInfo): boolean {
    return selectedDisplay.availableDisplayZoomFactors.length > 1;
  }

  /**
   * Returns true if the given mode is the best mode for the
   * |selectedDisplay|.
   */
  private isBestMode_(selectedDisplay: DisplayUnitInfo, mode: DisplayMode):
      boolean {
    if (!selectedDisplay.isInternal) {
      return mode.isNative;
    }

    // Things work differently for full HD devices(1080p). The best mode is
    // the one with 1.25 device scale factor and 0.8 ui scale.
    if (mode.heightInNativePixels === 1080) {
      return Math.abs(mode.uiScale! - 0.8) < 0.001 &&
          Math.abs(mode.deviceScaleFactor - 1.25) < 0.001;
    }

    return mode.uiScale === 1.0;
  }

  private getResolutionText_(): string {
    assertExists(this.selectedDisplay);
    if (this.selectedDisplay.modes.length === 0 ||
        this.currentSelectedModeIndex_ === -1) {
      // If currentSelectedModeIndex_ is -1, selectedDisplay and
      // |selectedModePref_.value| are not in sync.
      return this.i18n(
          'displayResolutionText', this.selectedDisplay.bounds.width.toString(),
          this.selectedDisplay.bounds.height.toString());
    }
    const mode =
        castExists(this.selectedDisplay.modes[this.selectedModePref_.value]);
    const widthStr = mode.width.toString();
    const heightStr = mode.height.toString();
    if (this.isBestMode_(this.selectedDisplay, mode)) {
      return this.i18n('displayResolutionTextBest', widthStr, heightStr);
    } else if (mode.isNative) {
      return this.i18n('displayResolutionTextNative', widthStr, heightStr);
    }
    return this.i18n('displayResolutionText', widthStr, heightStr);
  }

  /**
   * Updates the logical resolution text to be used for the display size
   * section
   * @param zoomFactor Current zoom factor applied on the selected display.
   */
  private updateLogicalResolutionText_(zoomFactor: number) {
    assertExists(this.selectedDisplay);
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
  }

  /**
   * Determines whether width and height should be swapped in the
   * Logical Resolution Text. Returns true if the longer edge of the
   * display's native pixels is different than the longer edge of the
   * display's current bounds.
   */
  private shouldSwapLogicalResolutionText_() {
    assertExists(this.selectedDisplay);
    const mode = this.selectedDisplay.modes[this.currentSelectedModeIndex_];
    const bounds = this.selectedDisplay.bounds;

    return bounds.width > bounds.height !==
        mode.widthInNativePixels > mode.heightInNativePixels;
  }

  /**
   * Handles the event where the display size slider is being dragged, i.e.
   * the mouse or tap has not been released.
   */
  private onDisplaySizeSliderDrag_() {
    if (!this.selectedDisplay) {
      return;
    }

    const slider = castExists(
        this.$.displaySizeSlider.shadowRoot!.querySelector<CrSliderElement>(
            '#slider'));
    const zoomFactor =
        (this.$.displaySizeSlider.ticks as SliderTick[])[slider.value].value;
    this.updateLogicalResolutionText_(zoomFactor);
  }

  /**
   * @param e |e.detail| is the id of the selected display.
   */
  private onSelectDisplay_(e: CustomEvent<string>) {
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
  }

  private onSelectDisplayTab_() {
    const {selected} = castExists(this.shadowRoot!.querySelector('cr-tabs'));
    if (this.selectedTab_ !== selected) {
      this.setSelectedDisplay_(this.displays[selected]);
    }
  }

  /**
   * Handles event when a touch calibration option is selected.
   */
  private onTouchCalibrationTap_() {
    getDisplayApi().showNativeTouchCalibration(this.selectedDisplay!.id);
  }

  /**
   * Handles the event when an option from display select menu is selected.
   */
  private updatePrimaryDisplay_(e: Event) {
    if (!this.selectedDisplay) {
      return;
    }
    if (this.selectedDisplay.id === this.primaryDisplayId) {
      return;
    }
    if (!(e.target as HTMLSelectElement).value) {
      return;
    }

    const properties: DisplayProperties = {
      isPrimary: true,
    };
    getDisplayApi()
        .setDisplayProperties(this.selectedDisplay.id, properties)
        .then(() => this.setPropertiesCallback_());
  }

  /**
   * Handles a change in the |selectedParentModePref| value triggered via the
   * observer.
   */
  private onSelectedParentModeChange_(newModeIndex: number) {
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
  }

  /**
   * Returns True if a new parentMode has been set and we have received an
   * update from Chrome.
   */
  private hasNewParentModeBeenSet(): boolean {
    if (this.currentSelectedParentModeIndex_ === -1) {
      return false;
    }

    return this.currentSelectedParentModeIndex_ !==
        this.selectedParentModePref_.value;
  }

  /**
   * Returns True if a new mode has been set and we have received an update
   * from Chrome.
   */
  private hasNewModeBeenSet(): boolean {
    if (this.currentSelectedModeIndex_ === -1) {
      return false;
    }

    if (this.currentSelectedParentModeIndex_ !==
        this.selectedParentModePref_.value) {
      return true;
    }

    return this.currentSelectedModeIndex_ !== this.selectedModePref_.value;
  }

  /**
   * Handles a change in |selectedModePref| triggered via the observer.
   */
  private onSelectedModeChange_(newModeIndex: number) {
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

    assertExists(this.selectedDisplay);
    const properties: DisplayProperties = {
      displayMode: this.selectedDisplay.modes[this.selectedModePref_.value],
    };

    this.refreshRateList_ = castExists(this.parentModeToRefreshRateMap_.get(
        this.selectedParentModePref_.value));
    getDisplayApi()
        .setDisplayProperties(this.selectedDisplay.id, properties)
        .then(() => this.setPropertiesCallback_());
  }

  /**
   * Triggerend when the display size slider changes its value. This only
   * occurs when the value is committed (i.e. not while the slider is being
   * dragged).
   */
  private onSelectedZoomChange_() {
    if (this.currentSelectedModeIndex_ === -1 || !this.selectedDisplay) {
      return;
    }

    const properties: DisplayProperties = {
      displayZoomFactor: this.selectedZoomPref_.value,
    };

    getDisplayApi()
        .setDisplayProperties(this.selectedDisplay.id, properties)
        .then(() => this.setPropertiesCallback_());
  }

  /**
   * Returns whether the option "Auto-rotate" is one of the shown options in
   * the rotation drop-down menu.
   */
  private showAutoRotateOption_(selectedDisplay: DisplayUnitInfo): boolean
      |undefined {
    return selectedDisplay.isAutoRotationAllowed;
  }

  private onOrientationChange_(event: Event) {
    const select = cast(event.target, HTMLSelectElement);
    const value = parseInt(select.value, 10);

    assertExists(this.selectedDisplay);
    assert(value !== -1 || this.selectedDisplay.isAutoRotationAllowed);

    const properties: DisplayProperties = {
      rotation: value,
    };
    getDisplayApi()
        .setDisplayProperties(this.selectedDisplay.id, properties)
        .then(() => this.setPropertiesCallback_());
  }

  private onMirroredTap_(event: Event) {
    // Blur the control so that when the transition animation completes and
    // the UI is focused, the control does not receive focus. crbug.com/785070
    (event.currentTarget as CrCheckboxElement).blur();

    const mirrorModeInfo: MirrorModeInfo = {
      mode: this.isMirrored_(this.displays) ? MirrorMode.OFF :
                                              MirrorMode.NORMAL,
    };
    getDisplayApi().setMirrorMode(mirrorModeInfo).then(() => {
      const error = chrome.runtime.lastError;
      if (error) {
        console.error('setMirrorMode Error: ' + error.message);
      }
    });
  }

  private onUnifiedDesktopTap_() {
    const properties: DisplayProperties = {
      isUnified: !this.unifiedDesktopMode_,
    };
    getDisplayApi()
        .setDisplayProperties(this.primaryDisplayId, properties)
        .then(() => this.setPropertiesCallback_());
  }

  private onOverscanTap_(e: Event) {
    e.preventDefault();
    this.overscanDisplayId = this.selectedDisplay!.id;
    this.showOverscanDialog_(true);
  }

  private onCloseOverscanDialog_() {
    focusWithoutInk(castExists(this.shadowRoot!.getElementById('overscan')));
  }

  private updateDisplayInfo_() {
    let displayIds = '';
    let primaryDisplay: DisplayUnitInfo|undefined = undefined;
    let selectedDisplay: DisplayUnitInfo|undefined = undefined;
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
  }

  private setPropertiesCallback_() {
    if (chrome.runtime.lastError) {
      console.error(
          'setDisplayProperties Error: ' + chrome.runtime.lastError.message);
    }
  }

  /**
   * Invoked when the status of Night Light or its schedule type are changed,
   * in order to update the schedule settings, such as whether to show the
   * custom schedule slider, and the schedule sub label.
   */
  private updateNightLightScheduleSettings_() {
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
  }

  private shouldShowArrangementSection_(): boolean {
    if (!this.displays) {
      return false;
    }
    return this.hasMultipleDisplays_() || this.isMirrored_(this.displays);
  }

  private onDisplaysChanged_() {
    flush();
    const displayLayout = this.shadowRoot!.querySelector('display-layout');
    if (displayLayout) {
      displayLayout.updateDisplays(
          this.displays, this.layouts, this.mirroringDestinationIds);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-display': SettingsDisplayElement;
  }
}

customElements.define(SettingsDisplayElement.is, SettingsDisplayElement);
