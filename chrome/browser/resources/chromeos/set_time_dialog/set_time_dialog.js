// Copyright 2019 The Chromium Authors. All rights reserved.
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

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_page_host_style_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SetTimeBrowserProxy, SetTimeBrowserProxyImpl} from './set_time_browser_proxy.js';

/**
 * @return {!Array<!{id: string, name: string, selected: Boolean}>} Items for
 *     the timezone select element.
 */
function getTimezoneItems() {
  const currentTimezoneId =
      /** @type {string} */ (loadTimeData.getValue('currentTimezoneId'));
  const timezoneList =
      /** @type {!Array} */ (loadTimeData.getValue('timezoneList'));
  return timezoneList.map(
      tz => ({id: tz[0], name: tz[1], selected: tz[0] === currentTimezoneId}));
}

/**
 * Builds date and time strings suitable for the values of HTML date and
 * time elements.
 * @param {!Date} date The date object to represent.
 * @return {!{date: string, time: string}} Date is an RFC 3339 formatted date
 *     and time is an HH:MM formatted time.
 * @private
 */
function dateToHtmlValues(date) {
  // Get the current time and subtract the timezone offset, so the
  // JSON string is in local time.
  const localDate = new Date(date);
  localDate.setMinutes(date.getMinutes() - date.getTimezoneOffset());
  return {
    date: localDate.toISOString().slice(0, 10),
    time: localDate.toISOString().slice(11, 16)
  };
}

/**
 * @return {string} Minimum date for the date picker in RFC 3339 format.
 */
function getMinDate() {
  // Start with the build date because we can't trust the clock. The build time
  // doesn't include a timezone, so subtract 1 day to get a safe minimum date.
  let minDate = new Date(loadTimeData.getValue('buildTime'));
  minDate.setDate(minDate.getDate() - 1);
  // Make sure the ostensible date is in range.
  const now = new Date();
  if (now < minDate)
    minDate = now;
  // Convert to string for date input min attribute.
  return dateToHtmlValues(minDate).date;
}

/**
 * @return {string} Maximum date for the date picker in RFC 3339 format.
 */
function getMaxDate() {
  // Set the max date to the build date plus 20 years.
  let maxDate = new Date(loadTimeData.getValue('buildTime'));
  maxDate.setFullYear(maxDate.getFullYear() + 20);
  // Make sure the ostensible date is in range.
  const now = new Date();
  if (now > maxDate)
    maxDate = now;
  // Convert to string for date input max attribute.
  return dateToHtmlValues(maxDate).date;
}

/**
 * Gets the current time in a different timezone.
 * @param {!string} timezoneId The timezone to be used to convert the time.
 * @return {Date} The converted time.
 */
function getDateInTimezone(timezoneId) {
  return new Date(new Date().toLocaleString('en-US', {timeZone: timezoneId}));
}

/**
 * Gives the time difference between two timezones.
 * @param {!string} firstTimezoneId The timezone on the left-hand size of the
 *     subtraction.
 * @param {!string} secondsTimezoneId The timezone on the right-hand side of the
 *     subtraction.
 * @return {number} Delta in milliseconds between the two timezones.
 */
function getTimezoneDelta(firstTimezoneId, secondsTimezoneId) {
  return getDateInTimezone(firstTimezoneId) -
      getDateInTimezone(secondsTimezoneId);
}

Polymer({
  is: 'set-time-dialog',

  _template: html`{__html_template__}`,

  // Remove listeners on detach.
  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Items to populate the timezone select.
     * @private
     */
    timezoneItems_: {
      type: Array,
      readonly: true,
      value: getTimezoneItems,
    },

    /**
     * Whether the timezone select element is visible.
     * @private
     */
    isTimezoneVisible_: {
      type: Boolean,
      readonly: true,
      value: () => loadTimeData.getValue('currentTimezoneId') != '',
    },

    /**
     * The minimum date allowed in the date picker.
     * @private
     */
    minDate_: {
      type: String,
      readonly: true,
      value: getMinDate,
    },

    /**
     * The maximum date allowed in the date picker.
     * @private
     */
    maxDate_: {
      type: String,
      readonly: true,
      value: getMaxDate,
    },

    /**
     * The last timezone selected.
     * @private
     */
    selectedTimezone_: {
      type: String,
      value: () =>
          /** @type {string} */ (loadTimeData.getValue('currentTimezoneId')),
    },
  },

  /**
   * Values for reverting inputs when the user's date/time is invalid. The
   * keys are element ids.
   * @private {{dateInput: string, timeInput: string}}
   */
  prevValues_: {dateInput: '', timeInput: ''},

  /**
   * ID of the setTimeout() used to refresh the current time.
   * @private {?number}
   */
  timeTimeoutId_: null,

  /** @private {?SetTimeBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = SetTimeBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.updateTime_(new Date());
  },

  /** @override */
  attached: function() {
    // Register listeners for updates from C++ code.
    this.addWebUIListener(
        'system-clock-updated', this.updateTime_.bind(this, new Date()));
    this.addWebUIListener(
        'system-timezone-changed', this.setTimezone_.bind(this));
    this.addWebUIListener('validation-complete', this.saveAndClose_.bind(this));

    this.browserProxy_.sendPageReady();

    /** @type {!CrDialogElement} */ (this.$.dialog).showModal();
  },

  /**
   * @return {!Date} The date that is currently displayed on the dialog.
   * @private
   */
  getInputTime_: function() {
    // Midnight of the current day in GMT.
    const date = this.$.dateInput.valueAsDate;
    // Add hours and minutes as set on the time input field.
    date.setMilliseconds(this.$.timeInput.valueAsNumber);
    // Add seconds from the system time, since the input fields only allow
    // setting hour and minute.
    date.setSeconds(date.getSeconds() + new Date().getSeconds());
    // Add timezone offset to get real time.
    date.setMinutes(date.getMinutes() + date.getTimezoneOffset());
    return date;
  },

  /**
   * @return {!number} Seconds since epoch representing the date on the dialog
   *     inputs.
   * @private
   */
  getInputTimeSinceEpoch_: function() {
    const now = this.getInputTime_();

    if (this.isTimezoneVisible_) {
      // Add timezone offset to get real time. This is only necessary when the
      // timezone was updated, which is only possible when the dropdown is
      // visible.
      const timezoneDelta = getTimezoneDelta(
          /** @type {string} */ (loadTimeData.getValue('currentTimezoneId')),
          this.selectedTimezone_);
      now.setMilliseconds(now.getMilliseconds() + timezoneDelta);
    }

    return Math.floor(now / 1000);
  },

  /**
   * Sets the current timezone.
   * @param {string} timezoneId The timezone ID to select.
   * @private
   */
  setTimezone_: function(timezoneId) {
    const timezoneSelect = this.$$('#timezoneSelect');
    assert(timezoneSelect.childElementCount > 0);
    timezoneSelect.value = timezoneId;

    const now = this.getInputTime_();
    const timezoneDelta = getTimezoneDelta(timezoneId, this.selectedTimezone_);
    now.setMilliseconds(now.getMilliseconds() + timezoneDelta);

    this.selectedTimezone_ = timezoneId;
    this.updateTime_(now);
  },

  /**
   * Updates the date/time controls time.
   * Called initially, then called again once a minute.
   * @param {!Date} newTime Time used to update the date/time controls.
   * @private
   */
  updateTime_: function(newTime) {
    // Only update time controls if neither is focused.
    if (document.activeElement.id != 'dateInput' &&
        document.activeElement.id != 'timeInput') {
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
  },

  /**
   * Sets the system time from the UI.
   * @private
   */
  applyTime_: function() {
    this.browserProxy_.setTimeInSeconds(this.getInputTimeSinceEpoch_());
  },

  /**
   * Called when focus is lost on date/time controls.
   * @param {!Event} e The blur event.
   * @private
   */
  onTimeBlur_: function(e) {
    if (e.target.validity.valid && e.target.value) {
      // Make this the new fallback time in case of future invalid input.
      this.prevValues_[e.target.id] = e.target.value;
    } else {
      // Restore previous value.
      e.target.value = this.prevValues_[e.target.id];
    }

    // Schedule periodic updates with the new time.
    this.updateTime_(this.getInputTime_());
  },

  /**
   * @param {!Event} e The change event.
   * @private
   */
  onTimezoneChange_: function(e) {
    this.setTimezone_(e.currentTarget.value);
  },

  /**
   * Called when the done button is clicked. Child accounts need parental
   * approval to change time, which requires an extra step after the button is
   * clicked. This method notifies the dialog delegate to start the approval
   * step, once the approval is granted the 'validation-complete' event is
   * triggered invoking saveAndClose_. For regular accounts, this step is
   * skipped and saveAndClose_ is called immediately after the button click.
   * @private
   */
  onDoneClick_: function() {
    this.browserProxy_.doneClicked(this.getInputTimeSinceEpoch_());
  },

  /** @private */
  saveAndClose_: function() {
    this.applyTime_();
    // Timezone change should only be applied when the UI displays timezone
    // setting. Otherwise |selectedTimezone_| will be empty/invalid.
    if (this.isTimezoneVisible_) {
      this.browserProxy_.setTimezone(this.selectedTimezone_);
    }
    this.browserProxy_.dialogClose();
  },
});
