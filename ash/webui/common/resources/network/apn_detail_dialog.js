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
import {assert} from '//resources/js/assert.js';
import {ApnAuthenticationType, ApnIpType, ApnProperties, ApnType, CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './apn_detail_dialog.html.js';
import {ApnDetailDialogMode} from './cellular_utils.js';
import {MojoInterfaceProviderImpl} from './mojo_interface_provider.js';

const AuthenticationTypes = [
  ApnAuthenticationType.kAutomatic,
  ApnAuthenticationType.kPap,
  ApnAuthenticationType.kChap,
];

const IpTypes = [
  ApnIpType.kAutomatic,
  ApnIpType.kIpv4,
  ApnIpType.kIpv6,
  ApnIpType.kIpv4Ipv6,
];

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
  onAddClicked_(event) {
    assert(this.guid);

    const apnProperties = /** @type {!ApnProperties} */ ({
      accessPointName: this.apn_,
      username: this.username_,
      password: this.password_,
      authenticationType: Number(this.selectedAuthType_),
      ipType: Number(this.selectedIpType_),
      // TODO(b/162365553): Check that ApnTypes is non-empty
      apnTypes: this.getSelectedApnTypes_(),
    });
    this.networkConfig_.createCustomApn(this.guid, apnProperties);

    this.$.apnDetailDialog.close();
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
        // TODO(b/162365553): Add edit mode for the apn detail dialog.
        return '';
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
   * @returns {boolean}
   */
  isUiElementDisabled_() {
    return this.mode === ApnDetailDialogMode.VIEW;
  }

  /**
   * @returns {boolean}
   */
  isUiElementVisible_() {
    return this.mode === ApnDetailDialogMode.CREATE;
  }
}

customElements.define(ApnDetailDialog.is, ApnDetailDialog);
