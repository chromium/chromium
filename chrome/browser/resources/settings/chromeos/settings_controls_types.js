// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Externs for files shared with Browser Settings and have been migrated to
 * TypeScript. Remove if ever CrOS Settings is migrated to TS.
 * @externs
 */

/** @interface */
function PrefControlMixinInterface() {}

/** @type {!chrome.settingsPrivate.PrefObject|undefined} */
PrefControlMixinInterface.prototype.pref;

/**
 * @interface
 * @extends {PrefControlMixinInterface}
 */
function SettingsBooleanControlMixinInterface() {}

/** @type {boolean} */
SettingsBooleanControlMixinInterface.prototype.checked;

/** @type {string} */
SettingsBooleanControlMixinInterface.prototype.label;

/** @return {boolean} */
SettingsBooleanControlMixinInterface.prototype.controlDisabled = function() {};

SettingsBooleanControlMixinInterface.prototype.notifyChangedByUserInteraction =
    function() {};
SettingsBooleanControlMixinInterface.prototype.resetToPrefValue = function() {};
SettingsBooleanControlMixinInterface.prototype.sendPrefChange = function() {};

/**
 * @constructor
 * @implements {SettingsBooleanControlMixinInterface}
 * @extends {HTMLElement}
 */
function SettingsToggleButtonElement() {}


/**
 * @typedef {{
 *   name: string,
 *   value: (number|string)
 * }}
 */
let DropdownMenuOption;

/**
 * @typedef {!Array<!DropdownMenuOption>}
 */
let DropdownMenuOptionList;

/** @interface */
function SettingsPrefsElement() {}

/** @param {string} key */
SettingsPrefsElement.prototype.refresh = function(key) {};

/**
 * @constructor
 * @extends {HTMLElement}
 */
function SettingsPersonalizationOptionsElement() {}

/** @return {?HTMLElement} */
SettingsPersonalizationOptionsElement.prototype.getDriveSuggestToggle =
    function() {};

/** @return {?HTMLElement} */
SettingsPersonalizationOptionsElement.prototype.getUrlCollectionToggle =
    function() {};

/** @return {?HTMLElement} */
SettingsPersonalizationOptionsElement.prototype.getSearchSuggestToggle =
    function() {};

/**
 * @constructor
 * @extends {HTMLElement}
 */
function SettingsSyncEncryptionOptionsElement() {}

/** @return {?HTMLElement} */
SettingsSyncEncryptionOptionsElement.prototype.getEncryptionsRadioButtons =
    function() {};


/**
 * @constructor
 * @extends {HTMLElement}
 */
function SettingsSyncPageElement() {}

/** @return {?SettingsPersonalizationOptionsElement} */
SettingsSyncPageElement.prototype.getPersonalizationOptions = function() {};

/** @return {?SettingsSyncEncryptionOptionsElement} */
SettingsSyncPageElement.prototype.getEncryptionOptions = function() {};
