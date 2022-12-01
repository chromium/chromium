// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element which will show a dialog to create, view or edit APNs.
 */

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/md_select.css.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './apn_detail_dialog.html.js';

/**
 * Possible authentication types shown in the APN detail dialog.
 * @enum {string}
 */
const AuthenticationType = {
  AUTOMATIC: 'auth_type_automatic',
  PAP: 'auth_type_pap',
  CHAP: 'auth_type_chap',
};

const AuthenticationTypes = [
  AuthenticationType.AUTOMATIC,
  AuthenticationType.PAP,
  AuthenticationType.CHAP,
];

/**
 * Possible IP types shown in the APN detail dialog.
 * @enum {string}
 */
const IpType = {
  AUTOMATIC: 'ip_type_automatic',
  IPv4: 'ip_type_ipv4',
  IPv6: 'ip_type_ipv6',
  IPv4_IPv6: 'ip_type_ipv4_ipv6',
};

const IpTypes = [
  IpType.AUTOMATIC,
  IpType.IPv4,
  IpType.IPv6,
  IpType.IPv4_IPv6,
];

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ApnDetailDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class ApnDetailDialog extends ApnDetailDialogElementBase {
  static get is() {
    return 'apn-detail-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
       * @private
       */
      selectedAuthType_: {
        type: String,
        value: AuthenticationTypes[0],
      },

      /**
       * @private
       */
      selectedIpType_: {
        type: String,
        value: IpTypes[0],
      },
    };
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
   * Returns the localized label for the auth type.
   * @param {AuthenticationType} type
   * @private
   */
  getAuthTypeLocalizedLabel_(type) {
    switch (type) {
      case AuthenticationType.AUTOMATIC:
        return this.i18n('apnDetailTypeAuto');
      case AuthenticationType.CHAP:
        return this.i18n('apnDetailAuthTypeCHAP');
      case AuthenticationType.PAP:
        return this.i18n('apnDetailAuthTypePAP');
    }
  }

  /**
   * Returns the localized label for the ip type.
   * @param {IpType} type
   * @private
   */
  getIpTypeLocalizedLabel_(type) {
    switch (type) {
      case IpType.AUTOMATIC:
        return this.i18n('apnDetailTypeAuto');
      case IpType.IPv4:
        return this.i18n('apnDetailIpTypeIpv4');
      case IpType.IPv6:
        return this.i18n('apnDetailIpTypeIpv6');
      case IpType.IPv4_IPv6:
        return this.i18n('apnDetailIpTypeIpv4_Ipv6');
    }
  }

  /**
   * @param {string} lhs
   * @param {string} rhs
   * @private
   */
  isEqual_(lhs, rhs) {
    return lhs === rhs;
  }
}

customElements.define(ApnDetailDialog.is, ApnDetailDialog);
