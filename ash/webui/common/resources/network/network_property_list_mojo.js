// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of network properties
 * in a list. This also supports editing fields inline for fields listed in
 * editFieldTypes.
 */
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import './cr_policy_network_indicator_mojo.js';
import './network_shared.css.js';

import {assert} from '//resources/ash/common/assert.js';
import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {ActivationStateType, SecurityType, SubjectAltName, VpnType} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {OncSource, PolicySource, PortalState} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {getTemplate} from './network_property_list_mojo.html.js';
import {FAKE_CREDENTIAL, OncMojo} from './onc_mojo.js';

Polymer({
  _template: getTemplate(),
  is: 'network-property-list-mojo',

  behaviors: [I18nBehavior, CrPolicyNetworkBehaviorMojo],

  properties: {
    /**
     * The dictionary containing the properties to display.
     * @type {!Object|undefined}
     */
    propertyDict: {
      type: Object,
      observer: 'onPropertyDictChanged_',
    },

    /**
     * Fields to display.
     * @type {!Array<string>}
     */
    fields: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * Edit type of editable fields. May contain a property for any field in
     * |fields|. Other properties will be ignored. Property values can be:
     *   'String' - A text input will be displayed.
     *   'StringArray' - A text input will be displayed that expects a comma
     *       separated list of strings.
     *   'Password' - A string with input type = password.
     * When a field changes, the 'property-change' event will be fired with
     * the field name and the new value provided in the event detail.
     * @type {!Object<string>}
     */
    editFieldTypes: {
      type: Object,
      value() {
        return {};
      },
    },

    /** Prefix used to look up property key translations. */
    prefix: {
      type: String,
      value: '',
    },

    /**
     * Whether all CrInputs are automatically read-only, and none are
     * editable by the user.
     */
    allFieldsReadOnly: {
      type: Boolean,
      value: true,
      readonly: true,
      observer: 'onAllFieldsReadOnlyChanged_',
    },

    disabled: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether any of the CrInputElements have been visibly focused since
     * |allFieldsReadOnly| becoming true.
     * @private
     */
    hasAnyInputFocused_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onAllFieldsReadOnlyChanged_() {
    if (this.allFieldsReadOnly) {
      return;
    }

    this.hasAnyInputFocused_ = false;

    // If this focus attempt fails (e.g. when other updates affect focus), the
    // call in onPropertyDictChanged_ will set the focus.
    setTimeout(() => {
      this.attemptToFocusFirstEditableCrInput_();
    });
  },

  /**
   * Since |this.propertyDict| may change multiple times after the
   * user |this.allFieldsReadOnly| becomes false (while editing the
   * properties of a connected network), the first CrInputElement should be
   * ready before it's focused.
   * @private
   */
  onPropertyDictChanged_() {
    // Do not proceed if the user has not opted for manual edit, or has
    // already made an edit.
    if (this.allFieldsReadOnly || this.hasAnyInputFocused_) {
      return;
    }

    this.attemptToFocusFirstEditableCrInput_();
  },

  /**
   * Attempts to focus the first non read-only CrInputElement.
   * @private
   */
  attemptToFocusFirstEditableCrInput_() {
    flush();

    const crInput = /** @type {?HTMLElement} */
        (this.shadowRoot.querySelector('cr-input:not([readonly])'));
    if (!crInput) {
      return;
    }

    // Note that |this.hasAnyInputFocused_| should not change here because a
    // CrInputElement's focus event may not properly fire before
    // |this.propertyDict| reaches steady state.
    /** @type {{focusInput: function():void}} */ (crInput).focusInput();
  },

  /**
   * Select the text contents of the input if
   * |this.allFieldsReadOnly| is true and the the CrInputElement
   * has not been focused before.
   * @param {!Event} e The input focus event.
   * @private
   */
  onInputFocused_(e) {
    if (this.allFieldsReadOnly) {
      return;
    }

    const crInput = /** @type {!HTMLElement} */ (e.target);
    // Subsequent focuses to the same CrInputElement after the first will not
    // select the entire text.
    if (crInput.getAttribute('edited') === 'true') {
      return;
    }

    // Set |edited| attribute to true so that the next time the user focuses
    // on the CrInputElement while |this.allFieldsReadOnly| is
    // still true, the entire contents are not selected.
    crInput.setAttribute('edited', true);
    crInput.select();
    this.hasAnyInputFocused_ = true;
  },

  /**
   * Event triggered when an input field changes. Fires a 'property-change'
   * event with the field (property) name set to the target id, and the value
   * set to the target input value.
   * @param {!Event} event The input change event.
   * @private
   */
  onValueChange_(event) {
    if (!this.propertyDict) {
      return;
    }
    const key = event.target.id;
    let curValue = this.getProperty_(key);
    if (typeof curValue === 'object' && !Array.isArray(curValue)) {
      // Extract the property from an ONC managed dictionary.
      curValue = OncMojo.getActiveValue(
          /** @type{!OncMojo.ManagedProperty} */ (curValue));
    }
    const newValue = this.getValueFromEditField_(key, event.target.value);
    if (newValue === curValue) {
      return;
    }
    this.fire('property-change', {field: key, value: newValue});
  },

  /**
   * Converts mojo keys to ONC keys. TODO(stevenjb): Remove this and update
   * string ids once everything is converted to mojo.
   * @param {string} key
   * @param {string=} opt_prefix
   * @return {string}
   * @private
   */
  getOncKey_(key, opt_prefix) {
    if (opt_prefix) {
      key = opt_prefix + key.charAt(0).toUpperCase() + key.slice(1);
    }
    let result = '';
    const subKeys = key.split('.');
    subKeys.forEach(subKey => {
      // Check for exceptions to CamelCase vs camelCase naming conventions.
      if (subKey === 'ipv4' || subKey === 'ipv6') {
        result += subKey;
      } else if (subKey === 'apn') {
        result += 'APN';
      } else if (subKey === 'ipAddress') {
        result += 'IPAddress';
      } else if (subKey === 'ipSec') {
        result += 'IPSec';
      } else if (subKey === 'l2tp') {
        result += 'L2TP';
      } else if (subKey === 'modelId') {
        result += 'ModelID';
      } else if (subKey === 'openVpn') {
        result += 'OpenVPN';
      } else if (subKey === 'otp') {
        result += 'OTP';
      } else if (subKey === 'ssid') {
        result += 'SSID';
      } else if (subKey === 'bssid') {
        result += 'BSSID';
      } else if (subKey === 'serverCa') {
        result += 'ServerCA';
      } else if (subKey === 'vpn') {
        result += 'VPN';
      } else if (subKey === 'wifi') {
        result += 'WiFi';
      } else if (subKey === 'iccid') {
        result += 'ICCID';
      } else if (subKey === 'imei') {
        result += 'IMEI';
      } else {
        result += subKey.charAt(0).toUpperCase() + subKey.slice(1);
      }
      result += '-';
    });
    return 'Onc' + result.slice(0, result.length - 1);
  },

  /**
   * @param {string} key The property key.
   * @return {string} The text to display for the property label.
   * @private
   */
  getPropertyLabel_(key) {
    const oncKey = this.getOncKey_(key, this.prefix);
    if (this.i18nExists(oncKey)) {
      return this.i18n(oncKey);
    }
    // We do not provide translations for every possible network property key.
    // For keys specific to a type, strip the type prefix.
    const result = this.prefix + key;
    for (const type of ['cellular', 'ethernet', 'tether', 'vpn', 'wifi']) {
      if (result.startsWith(type + '.')) {
        return result.substr(type.length + 1);
      }
    }
    return result;
  },

  /**
   * Generates a filter function dependent on propertyDict and editFieldTypes.
   * @return {!Object} A filter used by dom-repeat.
   * @private
   */
  computeFilter_() {
    return key => {
      if (this.editFieldTypes.hasOwnProperty(key)) {
        return true;
      }
      const value = this.getPropertyValue_(key);
      return value !== '';
    };
  },

  /**
   * @param {string} key The property key.
   * @return {boolean}
   * @private
   */
  isPropertyEditable_(key) {
    if (!this.propertyDict) {
      return false;
    }
    const property = this.getProperty_(key);
    if (property === undefined || property === null) {
      // Unspecified properties in policy configurations are not user
      // modifiable. https://crbug.com/819837.
      const source = this.propertyDict.source;
      return source !== OncSource.kUserPolicy &&
          source !== OncSource.kDevicePolicy;
    }
    return !this.isNetworkPolicyEnforced(property);
  },

  /**
   * @param {string} key The property key.
   * @return {boolean} True if the edit type for the key is a valid type.
   * @private
   */
  isEditType_(key) {
    const editType = this.editFieldTypes[key];
    return editType === 'String' || editType === 'StringArray' ||
        editType === 'Password';
  },

  /**
   * @param {string} key The property key.
   * @return {boolean}
   * @private
   */
  isEditable_(key) {
    return this.isEditType_(key) && this.isPropertyEditable_(key);
  },

  /**
   * @param {string} key The property key.
   * @return {boolean}
   * @private
   */
  showEditable_(key) {
    return this.isEditable_(key);
  },

  /**
   * @param {string} key The property key.
   * @return {string}
   * @private
   */
  getEditInputType_(key) {
    return this.editFieldTypes[key] === 'Password' ? 'password' : 'text';
  },

  /**
   * @param {string} key The property key.
   * @return {!OncMojo.ManagedProperty|undefined}
   * @private
   */
  getProperty_(key) {
    if (!this.propertyDict) {
      return undefined;
    }
    key = OncMojo.getManagedPropertyKey(key);
    const property = this.get(key, this.propertyDict);
    if (property === null || property === undefined) {
      return undefined;
    }
    return /** @type{!OncMojo.ManagedProperty}*/ (property);
  },

  /**
   * @param {string} key The property key.
   * @return {*} The managed property dictionary associated with |key|.
   * @private
   */
  getIndicatorProperty_(key) {
    if (!this.propertyDict) {
      return undefined;
    }
    const property = this.getProperty_(key);
    if ((property === undefined || property === null) &&
        this.propertyDict.source) {
      const policySource = OncMojo.getEnforcedPolicySourceFromOncSource(
          this.propertyDict.source);
      if (policySource !== PolicySource.kNone) {
        // If the dictionary is policy controlled, provide an empty property
        // object with the network policy source. See https://crbug.com/819837
        // for more info.
        return /** @type{!OncMojo.ManagedProperty} */ ({
          activeValue: '',
          policySource: policySource,
        });
      }
      // Otherwise just return undefined.
    }
    return property;
  },

  /**
   * @param {string} key The property key.
   * @return {string} The text to display for the property value.
   * @private
   */
  getPropertyValue_(key) {
    let value = this.getProperty_(key);
    if (value === undefined || value === null) {
      return '';
    }
    if (typeof value === 'object' && !Array.isArray(value)) {
      // Extract the property from an ONC managed dictionary
      value = OncMojo.getActiveValue(
          /** @type {!OncMojo.ManagedProperty} */ (value));
    }

    if (key === 'wifi.eap.subjectAltNameMatch') {
      return OncMojo.serializeSubjectAltNameMatch(
          /** @type {!Array<!SubjectAltName>} */ (value));
    }

    if (key === 'wifi.eap.domainSuffixMatch') {
      return OncMojo.serializeDomainSuffixMatch(
          /** @type {!Array<string>} */ (value));
    }

    if (Array.isArray(value)) {
      return value.join(', ');
    }

    const customValue = this.getCustomPropertyValue_(key, value);
    if (customValue) {
      return customValue;
    }
    if (typeof value === 'boolean') {
      return value.toString();
    }

    let valueStr;
    if (typeof value === 'number') {
      // Special case typed managed properties.
      if (key === 'cellular.activationState') {
        valueStr = OncMojo.getActivationStateTypeString(
            /** @type {!ActivationStateType}*/ (value));
      } else if (key === 'portalState') {
        valueStr = OncMojo.getPortalStateString(
            /** @type {!PortalState}*/ (value));
      } else if (key === 'vpn.type') {
        valueStr = OncMojo.getVpnTypeString(
            /** @type {!VpnType}*/ (value));
      } else if (key === 'wifi.security') {
        valueStr = OncMojo.getSecurityTypeString(
            /** @type {!SecurityType}*/ (value));
      } else {
        return value.toString();
      }
    } else {
      assert(typeof value === 'string');
      valueStr = /** @type {string} */ (value);
    }
    const oncKey = this.getOncKey_(key, this.prefix) + '_' + valueStr;
    if (this.i18nExists(oncKey)) {
      return this.i18n(oncKey);
    }
    return valueStr;
  },

  /**
   * @param {string} key The property key.
   * @return {string} CSS classes to apply to the property value container.
   * @private
   */
  getPropertyValueCssClasses_(key) {
    const classes = ['cr-secondary-text'];
    if (this.getPropertyValue_(key) === FAKE_CREDENTIAL) {
      classes.push('secure');
    }
    return classes.join(' ');
  },

  /**
   * Converts edit field values to the correct edit type.
   * @param {string} key The property key.
   * @param {*} fieldValue The value from the field.
   * @return {*}
   * @private
   */
  getValueFromEditField_(key, fieldValue) {
    const editType = this.editFieldTypes[key];
    if (editType === 'StringArray') {
      return fieldValue.toString().split(/, */);
    }
    return fieldValue;
  },

  /**
   * @param {string} key The property key.
   * @param {*} value The property value.
   * @return {string} The text to display for the property value. If the key
   *     does not correspond to a custom property, an empty string is returned.
   */
  getCustomPropertyValue_(key, value) {
    if (key === 'tether.batteryPercentage') {
      assert(typeof value === 'number');
      return this.i18n('OncTether-BatteryPercentage_Value', value.toString());
    }

    if (key === 'tether.signalStrength') {
      assert(typeof value === 'number');
      // Possible |signalStrength| values should be from 0 to 100. Add <=
      // checks for robustness.
      if (value === 0) {
        return this.i18n('OncTether-SignalStrength_None');
      }
      if (value <= 25) {
        return this.i18n('OncTether-SignalStrength_Low');
      }
      if (value <= 50) {
        return this.i18n('OncTether-SignalStrength_Medium');
      }
      return this.i18n('OncTether-SignalStrength_Strong');
    }

    if (key === 'tether.carrier') {
      assert(typeof value === 'string');
      return (!value || value === 'unknown-carrier') ?
          this.i18n('OncTether-Carrier_Unknown') :
          value;
    }

    return '';
  },
});
