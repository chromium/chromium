// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-time-picker' is a control that
 * displays time period options for 'settings-clear-browsing-data-dialog-v2'.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assertNotReachedCase} from 'chrome://resources/js/assert.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import {TimePeriod} from './clear_browsing_data_browser_proxy.js';
import {getTemplate} from './clear_browsing_data_time_picker.html.js';

/** The offset between the 'More' button and the dropdown menu. */
const MENU_VERTICAL_OFFSET_PX = 5;

interface TimePeriodOption {
  value: TimePeriod;
  label: string;
}

export function getTimePeriodString(
    timePeriod: TimePeriod, short: boolean = true) {
  switch (timePeriod) {
    case TimePeriod.LAST_15_MINUTES:
      return short ? loadTimeData.getString('clearPeriod15Min') :
                     loadTimeData.getString('clearPeriod15Minutes');
    case TimePeriod.LAST_HOUR:
      return loadTimeData.getString('clearPeriodHour');
    case TimePeriod.LAST_DAY:
      return loadTimeData.getString('clearPeriod24Hours');
    case TimePeriod.LAST_WEEK:
      return loadTimeData.getString('clearPeriod7Days');
    case TimePeriod.FOUR_WEEKS:
      return loadTimeData.getString('clearPeriod4Weeks');
    case TimePeriod.ALL_TIME:
      return loadTimeData.getString('clearPeriodEverything');
    default:
      assertNotReachedCase(timePeriod);
  }
}

export interface SettingsClearBrowsingDataTimePicker {
  $: {
    moreButton: HTMLButtonElement,
    moreTimePeriodsMenu: CrLazyRenderElement<CrActionMenuElement>,
    timePicker: HTMLElement,
  };
}

const SettingsClearBrowsingDataTimePickerBase = PrefsMixin(PolymerElement);

export class SettingsClearBrowsingDataTimePicker extends
    SettingsClearBrowsingDataTimePickerBase {
  static get is() {
    return 'settings-clear-browsing-data-time-picker';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedTimePeriod_: {
        type: Number,
        observer: 'onTimePeriodSelectionChanged_',
        value: TimePeriod.LAST_HOUR,
      },

      /**
       * The list of all available Time Periods ordered by duration in ascending
       * order.
       */
      allTimePeriodList_: {
        readOnly: true,
        type: Array,
        value: [
          TimePeriod.LAST_15_MINUTES,
          TimePeriod.LAST_HOUR,
          TimePeriod.LAST_DAY,
          TimePeriod.LAST_WEEK,
          TimePeriod.FOUR_WEEKS,
          TimePeriod.ALL_TIME,
        ],
      },

      /**
       * The list of Time Period options that are expanded by default, these
       * should include the currently selected time period.
       */
      expandedOptionList_: {
        type: Array,
        computed:
            'computeExpandedOptionList_(maxChipsShown_, selectedTimePeriod_)',
      },

      /**
       * The list of Time Period options that are hidden in the 'More' dropdown
       * menu.
       */
      moreOptionList_: {
        type: Array,
        computed: 'computeMoreOptionList_(expandedOptionList_)',
      },

      /**
         Maximum number of expanded chips to be shown, this should be less than
         or equal to the `allTimePeriodList_.length` and greater than 0.
       */
      maxChipsShown_: {
        type: Number,
        value: 4,
      },
    };
  }

  static get observers() {
    return [
      'onTimePeriodPrefUpdated_(prefs.browser.clear_data.time_period.value)',
    ];
  }

  declare private selectedTimePeriod_: TimePeriod;
  declare private allTimePeriodList_: TimePeriod[];
  declare private expandedOptionList_: TimePeriodOption[];
  declare private moreOptionList_: TimePeriodOption[];
  declare private maxChipsShown_: number;

  private onTimePeriodPrefUpdated_() {
    const timePeriodValue =
        this.getPref('browser.clear_data.time_period').value;

    if (timePeriodValue in TimePeriod &&
        timePeriodValue !== this.selectedTimePeriod_) {
      this.selectedTimePeriod_ = timePeriodValue;
    }
  }

  private onTimePeriodSelectionChanged_() {
    // Dispatch a |selected-time-period-change| event to notify that the
    // currently selected time period has changed due to an explicit user
    // selection or a pref change.
    this.dispatchEvent(new CustomEvent(
        'selected-time-period-change', {bubbles: true, composed: true}));
  }

  private computeExpandedOptionList_(): TimePeriodOption[] {
    const expandedOptionsList: TimePeriodOption[] = [];
    let selectedTimePeriodAdded = false;

    // Add the TimePeriods to the expanded options list up to maxChipsShow_, but
    // keep one slot for potentially the selected TimePeriod.
    for (let i = 0; i < this.maxChipsShown_ - 1; i++) {
      const timePeriod = this.allTimePeriodList_[i];
      selectedTimePeriodAdded ||= (timePeriod === this.selectedTimePeriod_);
      expandedOptionsList.push(
          {value: timePeriod, label: getTimePeriodString(timePeriod)});
    }

    // If the selected option is already added to the expandedOptionsList,
    // add another TimePeriod from the allTimePeriodList_ to fill the
    // maxChipsShown_ quota, otherwise add the selected time period.
    if (selectedTimePeriodAdded) {
      const timePeriod = this.allTimePeriodList_[this.maxChipsShown_ - 1];
      expandedOptionsList.push({
        value: timePeriod,
        label: getTimePeriodString(timePeriod),
      });
    } else {
      expandedOptionsList.push({
        value: this.selectedTimePeriod_,
        label: getTimePeriodString(this.selectedTimePeriod_),
      });
    }

    return expandedOptionsList;
  }

  private computeMoreOptionList_(): TimePeriodOption[] {
    const expandedTimePeriodsList: TimePeriod[] =
        this.expandedOptionList_.map((option) => option.value);
    const moreOptionsList: TimePeriodOption[] = [];

    // Add all options that were not included in the expandedOptionList_.
    this.allTimePeriodList_.forEach((timePeriod) => {
      if (!expandedTimePeriodsList.includes(timePeriod)) {
        moreOptionsList.push(
            {value: timePeriod, label: getTimePeriodString(timePeriod)});
      }
    });

    return moreOptionsList;
  }

  private onTimePickerDomChanged_() {
    // Decrease the number of visible chips if the current expanded chips' width
    // exceeds the maximum width.
    const timePicker = this.$.timePicker;
    if (timePicker.scrollWidth > timePicker.clientWidth) {
      this.maxChipsShown_--;
    }
  }
  private isTimePeriodSelected_(timePeriod: TimePeriod): boolean {
    return timePeriod === this.selectedTimePeriod_;
  }

  private onTimePeriodSelected_(event: DomRepeatEvent<TimePeriodOption>) {
    const newTimePeriod = event.model.item.value;

    if (newTimePeriod !== this.selectedTimePeriod_) {
      this.selectedTimePeriod_ = event.model.item.value;
    }
  }

  private onMenuTimePeriodSelected_(event: DomRepeatEvent<TimePeriodOption>) {
    this.onTimePeriodSelected_(event);
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu');
    if (actionMenu) {
      actionMenu.close();
    }
  }

  private onMoreTimePeriodsButtonClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;

    // Position the menu below the 'More' button with a slight offset.
    this.$.moreTimePeriodsMenu.get().showAt(target, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      top: target.getBoundingClientRect().bottom + MENU_VERTICAL_OFFSET_PX,
    });

    MetricsBrowserProxyImpl.getInstance().recordAction(
        'Settings.DeleteBrowsingData.TimePickerMoreClick');
  }

  private onMoreOptionsMenuClose_(e: Event) {
    // Stop propagation of the 'close' event so it doesn't close the outer
    // dialog.
    e.stopPropagation();
  }

  getSelectedTimePeriod(): TimePeriod {
    return this.selectedTimePeriod_;
  }

  sendPrefChange() {
    this.setPrefValue(
        'browser.clear_data.time_period', this.selectedTimePeriod_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-clear-browsing-data-time-picker':
        SettingsClearBrowsingDataTimePicker;
  }
}

customElements.define(
    SettingsClearBrowsingDataTimePicker.is,
    SettingsClearBrowsingDataTimePicker);
