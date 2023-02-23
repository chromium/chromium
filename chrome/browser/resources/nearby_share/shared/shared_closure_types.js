// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Externs for shared nearby share files. Only needed for the nearby share app
 * which is still using JS.
 * @externs
 */

/**
 * From
 * 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js'
 * @enum {number}
 */
const DataUsage = {

  kUnknown: 0,
  kOffline: 1,
  kOnline: 2,
  kWifiOnly: 3,
  MIN_VALUE: 0,
  MAX_VALUE: 3,
};

/**
 * From
 * 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js'
 * @enum {number}
 */
const Visibility = {

  kUnknown: 0,
  kNoOne: 1,
  kAllContacts: 2,
  kSelectedContacts: 3,
  MIN_VALUE: 0,
  MAX_VALUE: 3,
};

/**
 * From
 * 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js'
 * @enum {number}
 */
const FastInitiationNotificationState = {
  kEnabled: 0,
  kDisabledByUser: 1,
  kDisabledByFeature: 2,
  MIN_VALUE: 0,
  MAX_VALUE: 2,
};

/**
 * @typedef {{
 *            enabled:boolean,
 *            fastInitiationNotificationState: FastInitiationNotificationState,
 *            isFastInitiationHardwareSupported:boolean,
 *            deviceName:string,
 *            dataUsage:DataUsage,
 *            visibility:Visibility,
 *            allowedContacts:Array<string>,
 *            isOnboardingComplete:boolean,
 *          }}
 */
let NearbySettings;

/** @interface */
class NearbyShareSettingsMixinInterface {
  constructor() {
    /** @type {!NearbySettings} */
    this.settings;
  }

  onSettingsRetrieved() {}
}

/**
 * @typedef {{
 *    id: string,
 *    name: string,
 *    description: string,
 *    checked: boolean,
 * }}
 */
let NearbyVisibilityContact;

/**
 * @constructor
 * @extends {HTMLElement}
 */
class NearbyContactVisibilityElement {
  constructor() {
    /** @type {Array<!NearbyVisibilityContact>} */
    this.contacts;
    /** @type {?NearbySettings} */
    this.settings;
  }
}
/** @param {string|Array<string>} path */
NearbyContactVisibilityElement.prototype.set = function(path, value) {};
/**
 * @param {string|Array<string>} path
 */
NearbyContactVisibilityElement.prototype.get = function(path) {};
NearbyContactVisibilityElement.prototype.saveVisibilityAndAllowedContacts =
    function() {};

/**
 * @constructor
 * @extends {HTMLElement}
 */
function NearbyDeviceIconElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function NearbyDeviceElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function NearbyOnboardingOnePageElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function NearbyOnboardingPageElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
class NearbyPageTemplateElement {
  constructor() {
    /** @type {string} */
    this.actionButtonEventName;
    /** @type {string} */
    this.actionButtonLabel;
    /** @type {boolean} */
    this.actionDisabled;
    /** @type {string} */
    this.cancelButtonEventName;
    /** @type {string} */
    this.cancelButtonLabel;
    /** @type {boolean} */
    this.closeOnly;
    /** @type {string} */
    this.subTitle;
    /** @type {string} */
    this.utilityButtonEventName;
    /** @type {string} */
    this.utilityButtonLabel;
    /** @type {boolean} */
    this.utilityButtonOpenInNew;
  }
}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function NearbyPreviewElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function NearbyProgressElement() {}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function NearbyVisibilityPageElement() {}
