// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-time-picker' is a control that
 * displays time period options for 'settings-clear-browsing-data-dialog'.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {assertNotReachedCase} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';

import {TimePeriod} from './clear_browsing_data_browser_proxy.js';
import {getCss} from './clear_browsing_data_time_picker.css.js';
import {getHtml} from './clear_browsing_data_time_picker.html.js';

/** The offset between the 'More' button and the dropdown menu. */
const MENU_VERTICAL_OFFSET_PX = 5;

export interface TimePeriodOption {
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

export interface SettingsClearBrowsingDataTimePickerElement {
  $: {
    moreButton: HTMLButtonElement,
    moreTimePeriodsMenu: CrLazyRenderLitElement<CrActionMenuElement>,
    timePicker: HTMLElement,
  };
}

const SettingsClearBrowsingDataTimePickerElementBase =
    PrefServiceObserverMixinLit(CrLitElement);

export type ClearBrowsingDataTimePickerElement =
    SettingsClearBrowsingDataTimePickerElement;

export class SettingsClearBrowsingDataTimePickerElement extends
    SettingsClearBrowsingDataTimePickerElementBase {
  static get is() {
    return 'settings-clear-browsing-data-time-picker';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      selectedTimePeriod_: {type: Number},
      timePeriodPref_: {type: Object},
      expandedOptionList_: {type: Array},
      moreOptionList_: {type: Array},
      maxChipsShown_: {type: Number},
    };
  }

  private accessor selectedTimePeriod_: TimePeriod = TimePeriod.LAST_HOUR;

  /**
   * The list of all available Time Periods ordered by duration in ascending
   * order.
   */
  private allTimePeriodList_: TimePeriod[] = [
    TimePeriod.LAST_15_MINUTES,
    TimePeriod.LAST_HOUR,
    TimePeriod.LAST_DAY,
    TimePeriod.LAST_WEEK,
    TimePeriod.FOUR_WEEKS,
    TimePeriod.ALL_TIME,
  ];

  /**
   * The list of Time Period options that are expanded by default, these
   * should include the currently selected time period.
   */
  protected accessor expandedOptionList_: TimePeriodOption[] = [];

  /**
   * The list of Time Period options that are hidden in the 'More' dropdown
   * menu.
   */
  protected accessor moreOptionList_: TimePeriodOption[] = [];

  /**
   * Maximum number of expanded chips to be shown, this should be less than
   * or equal to the `allTimePeriodList_.length` and greater than 0.
   */
  private accessor maxChipsShown_: number = 4;

  private accessor timePeriodPref_:
      chrome.settingsPrivate.PrefObject<TimePeriod>|undefined;

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPrefs({
      'browser.clear_data.time_period': 'timePeriodPref_',
    });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('timePeriodPref_')) {
      this.onTimePeriodPrefUpdated_();
    }

    if (changedPrivateProperties.has('maxChipsShown_') ||
        changedPrivateProperties.has('selectedTimePeriod_') ||
        changedPrivateProperties.has('timePeriodPref_')) {
      this.expandedOptionList_ = this.computeExpandedOptionList_();
      this.moreOptionList_ = this.computeMoreOptionList_();
    }

    if (changedPrivateProperties.has('selectedTimePeriod_') &&
        changedPrivateProperties.get('selectedTimePeriod_') !== undefined) {
      this.onTimePeriodSelectionChanged_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('expandedOptionList_')) {
      // Decrease the number of visible chips if the current expanded chips'
      // width exceeds the maximum width.
      const timePicker = this.$.timePicker;
      if (timePicker.scrollWidth > timePicker.clientWidth &&
          this.maxChipsShown_ > 1) {
        this.maxChipsShown_--;
      }
    }
  }

  private onTimePeriodPrefUpdated_() {
    if (this.timePeriodPref_ === undefined) {
      return;
    }

    const timePeriodValue = this.timePeriodPref_.value;
    if (timePeriodValue in TimePeriod &&
        timePeriodValue !== this.selectedTimePeriod_) {
      this.selectedTimePeriod_ = timePeriodValue;
    }
  }

  private onTimePeriodSelectionChanged_() {
    // Dispatch a |selected-time-period-change| event to notify that the
    // currently selected time period has changed due to an explicit user
    // selection or a pref change.
    this.fire('selected-time-period-change');
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

  protected isTimePeriodSelected_(timePeriod: TimePeriod): boolean {
    return timePeriod === this.selectedTimePeriod_;
  }

  protected onTimePeriodClick_(event: Event) {
    const target = event.currentTarget as HTMLElement;
    const newTimePeriod = Number(target.dataset['timePeriod']) as TimePeriod;

    if (newTimePeriod !== this.selectedTimePeriod_) {
      this.selectedTimePeriod_ = newTimePeriod;
    }
  }

  protected onMenuTimePeriodClick_(event: Event) {
    this.onTimePeriodClick_(event);
    const actionMenu = this.shadowRoot.querySelector('cr-action-menu');
    if (actionMenu) {
      actionMenu.close();
    }
  }

  protected onMoreTimePeriodsButtonClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;

    // Position the menu below the 'More' button with a slight offset.
    this.$.moreTimePeriodsMenu.get().showAt(target, {
      anchorAlignmentX: AnchorAlignment.BEFORE_END,
      top: target.getBoundingClientRect().bottom + MENU_VERTICAL_OFFSET_PX,
    });

    MetricsBrowserProxyImpl.getInstance().recordAction(
        'Settings.DeleteBrowsingData.TimePickerMoreClick');
  }

  protected onMoreOptionsMenuClose_(e: Event) {
    // Stop propagation of the 'close' event so it doesn't close the outer
    // dialog.
    e.stopPropagation();
  }

  getSelectedTimePeriod(): TimePeriod {
    return this.selectedTimePeriod_;
  }

  sendPrefChange() {
    PrefService.getInstance().setPrefValue(
        'browser.clear_data.time_period', this.selectedTimePeriod_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-clear-browsing-data-time-picker':
        SettingsClearBrowsingDataTimePickerElement;
  }
}

customElements.define(
    SettingsClearBrowsingDataTimePickerElement.is,
    SettingsClearBrowsingDataTimePickerElement);
