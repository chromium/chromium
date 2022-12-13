// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element which will show a dialog to create, view or edit APNs.
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/md_select.css.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {assert} from '//resources/ash/common/assert.js';
import {ApnAuthenticationType, ApnIpType, ApnProperties, ApnType, CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './apn_detail_dialog.html.js';
import {ApnDetailDialogMode} from './cellular_utils.js';
import {MojoInterfaceProviderImpl} from './mojo_interface_provider.js';

/** @type {Array} */
const AuthenticationTypes = [
  ApnAuthenticationType.kAutomatic,
  ApnAuthenticationType.kPap,
  ApnAuthenticationType.kChap,
];

/** @type {Array} */
const IpTypes = [
  ApnIpType.kAutomatic,
  ApnIpType.kIpv4,
  ApnIpType.kIpv6,
  ApnIpType.kIpv4Ipv6,
];

/** @enum {number} */
const UiElement = {
  INPUT: 0,
  ACTION_BUTTON: 1,
  DONE_BUTTON: 2,
};

/**
 * Regular expression that is used to test for non-ASCII characters.
 * @type {RegExp}
 * @private
 */
const APN_NON_ASCII_REGEX = /[^\x00-\x7f]+/;

/**
 * Maximum allowed length of the APN input field.
 * @type {number}
 * @private
 */
const MAX_APN_INPUT_LENGTH = 63;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ApnDetailDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ApnDetailDialog extends ApnDetailDialogElementBase {
  static get is() {
    return 'apn-detail-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {ApnProperties|undefined} */
      apnProperties: {
        type: Object,
        observer: 'onApnPropertiesUpdated_',
      },

      /** @type {ApnDetailDialogMode} */
      mode: {
        type: Object,
        value: ApnDetailDialogMode.CREATE,
      },

      guid: {type: String},

      /** @private */
      advancedSettingsExpanded_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      AuthenticationTypes: {
        type: Array,
        value: AuthenticationTypes,
        readOnly: true,
      },

      /** @private */
      IpTypes: {
        type: Array,
        value: IpTypes,
        readOnly: true,
      },

      /**
       * Enum used as an ID for specific UI elements.
       * @type {!UiElement}
       * @private
       */
      UiElement: {
        type: Object,
        value: UiElement,
      },

      /** @private */
      selectedAuthType_: {
        type: String,
        value: AuthenticationTypes[0].toString(),
      },

      /** @private */
      selectedIpType_: {
        type: String,
        value: IpTypes[0].toString(),
      },

      /** @private */
      apn_: {
        type: String,
        value: '',
        observer: 'onApnValueChanged_',
      },

      /** @private */
      username_: {
        type: String,
        value: '',
      },

      /** @private */
      password_: {
        type: String,
        value: '',
      },

      /** @private */
      isDefaultApnType_: {
        type: Boolean,
        value: true,
      },

      /** @private */
      isAttachApnType_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      isApnInputInvalid_: {
        type: Boolean,
        value: false,
        computed:
            'computeIsApnInputInvalid_(apn_, isMaxApnInputLengthReached_)',
      },

      /** @private */
      isMaxApnInputLengthReached_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!CrosNetworkConfigRemote} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * Observer method used to fill the apn detail dialog, with the provided apn.
   * @private
   */
  onApnPropertiesUpdated_() {
    this.apn_ = /** @type {string}*/ (this.apnProperties.accessPointName);
    this.username_ = /** @type {string}*/ (this.apnProperties.username);
    this.password_ = /** @type {string}*/ (this.apnProperties.password);
    this.selectedIpType_ = this.apnProperties.ipType.toString();
    this.selectedAuthType_ = this.apnProperties.authenticationType.toString();
    this.isDefaultApnType_ = false;
    this.isAttachApnType_ = false;

    for (const apnType of this.apnProperties.apnTypes) {
      if (apnType === ApnType.kDefault) {
        this.isDefaultApnType_ = true;
      } else if (apnType === ApnType.kAttach) {
        this.isAttachApnType_ = true;
      }
    }
  }

  /**
   * Observer for apn_ that is used for detecting whether the max apn length
   * was reached or not and truncating it to MAX_APN_INPUT_LENGTH if so.
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onApnValueChanged_(newValue, oldValue) {
    if (oldValue) {
      // If oldValue.length > MAX_APN_INPUT_LENGTH, the user attempted to
      // enter more than the max limit, this method was called and it was
      // truncated, and then this method was called one more time.
      this.isMaxApnInputLengthReached_ = oldValue.length > MAX_APN_INPUT_LENGTH;
    } else {
      this.isMaxApnInputLengthReached_ = false;
    }

    // Truncate the name to MAX_INPUT_LENGTH.
    this.apn_ = this.apn_.substring(0, MAX_APN_INPUT_LENGTH);
  }

  /** @private */
  computeIsApnInputInvalid_() {
    return this.isMaxApnInputLengthReached_ ||
        APN_NON_ASCII_REGEX.test(this.apn_);
  }

  /** @private */
  getApnErrorMessage_() {
    if (!this.isApnInputInvalid_) {
      return '';
    }
    if (this.isMaxApnInputLengthReached_) {
      // TODO(b/162365553): Replace with real string when available
      return `APN cannot have more than 63 characters`;
    }
    // TODO(b/162365553): Replace with real string when available
    return 'APN cannot have non-ASCII characters';
  }

  /**
   * @param {!Event} event
   * @private
   */
  onCancelClicked_(event) {
    event.stopPropagation();
    if (this.$.apnDetailDialog.open) {
      this.$.apnDetailDialog.close();
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onActionButtonClicked_(event) {
    assert(this.guid);
    assert(this.mode !== ApnDetailDialogMode.VIEW);
    if (this.mode === ApnDetailDialogMode.CREATE) {
      // Note: apnProperties is undefined when we are in the create mode.
      assert(!this.apnProperties);
      this.networkConfig_.createCustomApn(this.guid, this.getApnProperties_());
    } else if (this.mode === ApnDetailDialogMode.EDIT) {
      assert(!!this.apnProperties.id);
      this.networkConfig_.modifyCustomApn(
          this.guid, this.getApnProperties_(this.apnProperties));
    }
    this.$.apnDetailDialog.close();
  }

  /**
   * @return {!ApnProperties}
   * @private
   */
  getApnProperties_(apnProperties = {}) {
    apnProperties.accessPointName = this.apn_;
    apnProperties.username = this.username_;
    apnProperties.password = this.password_;
    apnProperties.authenticationType = Number(this.selectedAuthType_);
    apnProperties.ipType = Number(this.selectedIpType_);
    // TODO(b/162365553): Check that ApnTypes is non-empty
    apnProperties.apnTypes = this.getSelectedApnTypes_();
    return /** @type {!ApnProperties}*/ (apnProperties);
  }

  /**
   * @return {string}
   * @private
   */
  getActionButtonTitle_() {
    if (this.mode === ApnDetailDialogMode.EDIT) {
      return this.i18n('save');
    }
    return this.i18n('add');
  }
  /**
   * @private
   */
  getDialogTitle_() {
    switch (this.mode) {
      case ApnDetailDialogMode.CREATE:
        return this.i18n('apnDetailAddApnDialogTitle');
      case ApnDetailDialogMode.VIEW:
        return this.i18n('apnDetailViewApnDialogTitle');
      case ApnDetailDialogMode.EDIT:
        return this.i18n('apnDetailEditApnDialogTitle');
    }
  }
  /**
   * Maps the checkboxes to an array of {@link ApnType}.
   * @returns {Array<ApnType>}
   * @private
   */
  getSelectedApnTypes_() {
    const apnTypes = [];
    if (this.isDefaultApnType_) {
      apnTypes.push(ApnType.kDefault);
    }

    if (this.isAttachApnType_) {
      apnTypes.push(ApnType.kAttach);
    }
    return apnTypes;
  }

  /**
   * Returns the localized label for the auth type.
   * @param {ApnAuthenticationType} type
   * @private
   */
  getAuthTypeLocalizedLabel_(type) {
    switch (type) {
      case ApnAuthenticationType.kAutomatic:
        return this.i18n('apnDetailTypeAuto');
      case ApnAuthenticationType.kChap:
        return this.i18n('apnDetailAuthTypeCHAP');
      case ApnAuthenticationType.kPap:
        return this.i18n('apnDetailAuthTypePAP');
    }
  }

  /**
   * Returns the localized label for the ip type.
   * @param {ApnIpType} type
   * @private
   */
  getIpTypeLocalizedLabel_(type) {
    switch (type) {
      case ApnIpType.kAutomatic:
        return this.i18n('apnDetailTypeAuto');
      case ApnIpType.kIpv4:
        return this.i18n('apnDetailIpTypeIpv4');
      case ApnIpType.kIpv6:
        return this.i18n('apnDetailIpTypeIpv6');
      case ApnIpType.kIpv4Ipv6:
        return this.i18n('apnDetailIpTypeIpv4_Ipv6');
    }
  }

  /**
   * @param {number} item
   */
  isSelectedIpType_(item) {
    return Number(this.selectedIpType_) === item;
  }

  /**
   * @param {number} item
   */
  isSelectedAuthType_(item) {
    return Number(this.selectedAuthType_) === item;
  }

  /**
   * @param {!UiElement} uiElement
   * @returns {boolean}
   * @private
   */
  isUiElementDisabled_(uiElement) {
    switch (uiElement) {
      case UiElement.INPUT:
        return this.mode === ApnDetailDialogMode.VIEW;
      case UiElement.ACTION_BUTTON:
        return this.apn_.length === 0 || this.isApnInputInvalid_;
    }
    return false;
  }

  /**
   * @param {!UiElement} uiElement
   * @returns {boolean}
   * @private
   */
  isUiElementVisible_(uiElement) {
    switch (uiElement) {
      case UiElement.DONE_BUTTON:
        return this.mode === ApnDetailDialogMode.VIEW;
      case UiElement.ACTION_BUTTON:
        return this.mode === ApnDetailDialogMode.CREATE ||
            this.mode === ApnDetailDialogMode.EDIT;
    }
    return true;
  }
}

customElements.define(ApnDetailDialog.is, ApnDetailDialog);
