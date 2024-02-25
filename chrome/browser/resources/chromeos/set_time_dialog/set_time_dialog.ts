// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'set-time-dialog' handles a dialog to check and set system time. It can also
 * include a timezone dropdown if timezoneId is provided.
 *
 * 'set-time-dialog' uses the system time to populate the controls initially and
 * update them as the system time or timezone changes, and notifies Chrome
 * when the user changes the time or timezone.
 */

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert, assertInstanceof} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SetTimeBrowserProxy, SetTimeBrowserProxyImpl} from './set_time_browser_proxy.js';
import {getTemplate} from './set_time_dialog.html.js';

type TimezoneList = Array<[id: string, name: string]>;

interface TimezoneListItem {
  id: string;
  name: string;
  selected: boolean;
}

function getTimezoneItems(): TimezoneListItem[] {
  const currentTimezoneId = loadTimeData.getString('currentTimezoneId');
  const timezoneList = loadTimeData.getValue('timezoneList') as TimezoneList;
  return timezoneList.map(
      tz => ({id: tz[0], name: tz[1], selected: tz[0] === currentTimezoneId}));
}

/**
 * Builds date and time strings suitable for the values of HTML date and
 * time elements.
 * @param date The date object to represent.
 * @return An object containing 2 properties:
 *   Date is an RFC 3339 formatted date
 *   Time is an HH:MM formatted time.
 */
function dateToHtmlValues(date: Date): {date: string, time: string} {
  // Get the current time and subtract the timezone offset, so the
  // JSON string is in local time.
  const localDate = new Date(date);
  localDate.setMinutes(date.getMinutes() - date.getTimezoneOffset());
  return {
    date: localDate.toISOString().slice(0, 10),
    time: localDate.toISOString().slice(11, 16),
  };
}

/**
 * @return Minimum date for the date picker in RFC 3339 format.
 */
function getMinDate(): string {
  // Start with the build date because we can't trust the clock. The build time
  // doesn't include a timezone, so subtract 1 day to get a safe minimum date.
  let minDate = new Date(loadTimeData.getValue('buildTime'));
  minDate.setDate(minDate.getDate() - 1);
  // Make sure the ostensible date is in range.
  const now = new Date();
  if (now < minDate) {
    minDate = now;
  }
  // Convert to string for date input min attribute.
  return dateToHtmlValues(minDate).date;
}

/**
 * @return Maximum date for the date picker in RFC 3339 format.
 */
function getMaxDate(): string {
  // Set the max date to the build date plus 20 years.
  let maxDate = new Date(loadTimeData.getValue('buildTime'));
  maxDate.setFullYear(maxDate.getFullYear() + 20);
  // Make sure the ostensible date is in range.
  const now = new Date();
  if (now > maxDate) {
    maxDate = now;
  }
  // Convert to string for date input max attribute.
  return dateToHtmlValues(maxDate).date;
}

/**
 * Returns the current time converted to the timezone of the give `timezoneId`.
 */
function getDateInTimezone(timezoneId: string): Date {
  return new Date(new Date()
                      .toLocaleString('en-US', {timeZone: timezoneId})
                      .replace('\u202f', ' '));
}

/**
 * Returns the time difference (in milliseconds) between two timezones.
 */
function getTimezoneDelta(
    firstTimezoneId: string, secondTimezoneId: string): number {
  return getDateInTimezone(firstTimezoneId).getTime() -
      getDateInTimezone(secondTimezoneId).getTime();
}

interface SetTimeDialogElement {
  $: {
    dateInput: HTMLInputElement,
    dialog: CrDialogElement,
    timeInput: HTMLInputElement,
  };
}

const SetTimeDialogBase = WebUiListenerMixin(PolymerElement);

class SetTimeDialogElement extends SetTimeDialogBase {
  static get is() {
    return 'set-time-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Items to populate the timezone select.
       */
      timezoneItems_: {
        type: Array,
        readOnly: true,
        value: getTimezoneItems,
      },

      /**
       * Whether the timezone select element is visible.
       */
      isTimezoneVisible_: {
        type: Boolean,
        readOnly: true,
        value() {
          return loadTimeData.getBoolean('showTimezone');
        },
      },

      /**
       * The minimum date allowed in the date picker.
       */
      minDate_: {
        type: String,
        readOnly: true,
        value: getMinDate,
      },

      /**
       * The maximum date allowed in the date picker.
       */
      maxDate_: {
        type: String,
        readOnly: true,
        value: getMaxDate,
      },

      selectedTimezone_: {
        type: String,
        value() {
          return loadTimeData.getString('currentTimezoneId');
        },
      },
    };
  }

  private browserProxy_: SetTimeBrowserProxy =
      SetTimeBrowserProxyImpl.getInstance();
  private readonly isTimezoneVisible_: boolean;
  private readonly maxDate_: string;
  private readonly minDate_: string;
  private prevValues_:
      {dateInput: string, timeInput: string} = {dateInput: '', timeInput: ''};
  private selectedTimezone_: string;
  private readonly timezoneItems_: TimezoneListItem[];
  /** ID of the timeout used to refresh the current time. */
  private timeTimeoutId_: number|null = null;

  override connectedCallback(): void {
    super.connectedCallback();

    // Register listeners for updates from C++ code.
    this.addWebUiListener(
        'system-clock-updated', this.updateTime_.bind(this, new Date()));
    this.addWebUiListener(
        'system-timezone-changed', this.setTimezone_.bind(this));
    this.addWebUiListener('validation-complete', this.saveAndClose_.bind(this));

    this.browserProxy_.sendPageReady();

    this.$.dialog.showModal();
  }

  override ready(): void {
    super.ready();
    this.updateTime_(new Date());
  }

  private getInputTime_(): Date {
    // Midnight of the current day in GMT.
    const date = this.$.dateInput.valueAsDate;
    assert(date);

    // Add hours and minutes as set on the time input field.
    date.setMilliseconds(this.$.timeInput.valueAsNumber);
    // Add seconds from the system time, since the input fields only allow
    // setting hour and minute.
    date.setSeconds(date.getSeconds() + new Date().getSeconds());
    // Add timezone offset to get real time.
    date.setMinutes(date.getMinutes() + date.getTimezoneOffset());
    return date;
  }

  /**
   * @return Seconds since epoch representing the date on the dialog inputs.
   */
  private getInputTimeSinceEpoch_(): number {
    const now = this.getInputTime_();

    if (this.isTimezoneVisible_) {
      // Add timezone offset to get real time. This is only necessary when the
      // timezone was updated, which is only possible when the dropdown is
      // visible.
      const timezoneDelta = getTimezoneDelta(
          loadTimeData.getString('currentTimezoneId'), this.selectedTimezone_);
      now.setMilliseconds(now.getMilliseconds() + timezoneDelta);
    }

    return Math.floor(now.getTime() / 1000);
  }

  private setTimezone_(timezoneId: string): void {
    if (this.isTimezoneVisible_) {
      const timezoneSelect =
          this.shadowRoot!.querySelector<HTMLSelectElement>('#timezoneSelect');
      assert(timezoneSelect);
      assert(timezoneSelect.childElementCount > 0);
      timezoneSelect.value = timezoneId;
    }

    const now = this.getInputTime_();
    const timezoneDelta = getTimezoneDelta(timezoneId, this.selectedTimezone_);
    now.setMilliseconds(now.getMilliseconds() + timezoneDelta);

    this.selectedTimezone_ = timezoneId;
    this.updateTime_(now);
  }

  /**
   * Updates the date/time controls time.
   * Called initially, then called again once a minute.
   * @param newTime Time used to update the date/time controls.
   */
  private updateTime_(newTime: Date): void {
    // Only update time controls if neither is focused.
    if (document.activeElement!.id !== 'dateInput' &&
        document.activeElement!.id !== 'timeInput') {
      const htmlValues = dateToHtmlValues(newTime);
      this.prevValues_.dateInput = this.$.dateInput.value = htmlValues.date;
      this.prevValues_.timeInput = this.$.timeInput.value = htmlValues.time;
    }

    if (this.timeTimeoutId_) {
      window.clearTimeout(this.timeTimeoutId_);
    }

    // Start timer to update these inputs every minute.
    const secondsRemaining = 60 - newTime.getSeconds();
    const nextTime =
        new Date(newTime.setSeconds(newTime.getSeconds() + secondsRemaining));
    this.timeTimeoutId_ = window.setTimeout(
        this.updateTime_.bind(this, nextTime), secondsRemaining * 1000);
  }

  /**
   * Sets the system time from the UI.
   */
  private applyTime_(): void {
    this.browserProxy_.setTimeInSeconds(this.getInputTimeSinceEpoch_());
  }

  /**
   * Called when focus is lost on date/time controls.
   */
  private onInputBlur_(e: Event): void {
    const inputEl = e.target;
    assertInstanceof(inputEl, HTMLInputElement);

    const valueKey = inputEl.type === 'date' ? 'dateInput' : 'timeInput';
    if (inputEl.value && inputEl.validity.valid) {
      // Make this the new fallback time in case of future invalid input.
      this.prevValues_[valueKey] = inputEl.value;
    } else {
      // Restore previous value.
      inputEl.value = this.prevValues_[valueKey];
    }

    // Schedule periodic updates with the new time.
    this.updateTime_(this.getInputTime_());
  }

  private onTimezoneChange_(e: Event): void {
    const selectEl = e.currentTarget;
    assertInstanceof(selectEl, HTMLSelectElement);
    this.setTimezone_(selectEl.value);
  }

  /**
   * Called when the done button is clicked. Child accounts need parental
   * approval to change time, which requires an extra step after the button is
   * clicked. This method notifies the dialog delegate to start the approval
   * step, once the approval is granted the 'validation-complete' event is
   * triggered invoking `saveAndClose_`. For regular accounts, this step is
   * skipped and `saveAndClose_` is called immediately after the button click.
   */
  private onDoneClick_(): void {
    this.browserProxy_.doneClicked(this.getInputTimeSinceEpoch_());
  }

  private saveAndClose_(): void {
    this.applyTime_();
    // Timezone change should only be applied when the UI displays timezone
    // setting. Otherwise `selectedTimezone_` will be empty/invalid.
    if (this.isTimezoneVisible_) {
      this.browserProxy_.setTimezone(this.selectedTimezone_);
    }
    this.browserProxy_.dialogClose();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SetTimeDialogElement.is]: SetTimeDialogElement;
  }
}

customElements.define(SetTimeDialogElement.is, SetTimeDialogElement);
