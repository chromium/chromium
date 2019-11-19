// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from 'chrome://resources/js/assert.m.js';
import {getInstance, Setting, Settings} from '../data/model.js';

/** @polymerBehavior */
export const SettingsBehavior = {
  properties: {
    /** @type {Settings} */
    settings: Object,
  },

  /**
   * @param {string} settingName Name of the setting to get.
   * @return {Setting} The setting object.
   */
  getSetting: function(settingName) {
    return getInstance().getSetting(settingName);
  },

  /**
   * @param {string} settingName Name of the setting to get the value for.
   * @return {*} The value of the setting, accounting for availability.
   */
  getSettingValue: function(settingName) {
    return getInstance().getSettingValue(settingName);
  },

  /**
   * Sets settings.settingName.value to |value|, unless updating the setting is
   * disallowed by enterprise policy. Fires preview-setting-changed and
   * sticky-setting-changed events if the update impacts the preview or requires
   * an update to sticky settings.
   * @param {string} settingName Name of the setting to set
   * @param {*} value The value to set the setting to.
   * @param {boolean=} noSticky Whether to avoid stickying the setting. Defaults
   *     to false.
   */
  setSetting: function(settingName, value, noSticky) {
    getInstance().setSetting(settingName, value, noSticky);
  },

  /**
   * @param {string} settingName Name of the setting to set
   * @param {number} start
   * @param {number} end
   * @param {*} newValue The value to add (if any).
   * @param {boolean=} noSticky Whether to avoid stickying the setting. Defaults
   *     to false.
   */
  setSettingSplice: function(settingName, start, end, newValue, noSticky) {
    getInstance().setSettingSplice(settingName, start, end, newValue, noSticky);
  },

  /**
   * Sets the validity of |settingName| to |valid|. If the validity is changed,
   * fires a setting-valid-changed event.
   * @param {string} settingName Name of the setting to set
   * @param {boolean} valid Whether the setting value is currently valid.
   */
  setSettingValid: function(settingName, valid) {
    getInstance().setSettingValid(settingName, valid);
  },
};
