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
export const AUTHENTICATION_TYPES = {
  AUTOMATIC: 'auth_type_automatic',
  PAP: 'auth_type_pap',
  CHAP: 'auth_type_chap',
};

/**
 * Possible IP types shown in the APN detail dialog.
 * @enum {string}
 */
export const IP_TYPES = {
  AUTOMATIC: 'ip_type_automatic',
  IPv4: 'ip_type_ipv4',
  IPv6: 'ip_type_ipv6',
  IPv4_IPv6: 'ip_type_ipv4_ipv6',
};

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
      authTypes_: {
        type: Array,
        value: [
          AUTHENTICATION_TYPES.AUTOMATIC,
          AUTHENTICATION_TYPES.PAP,
          AUTHENTICATION_TYPES.CHAP,
        ],
        readOnly: true,
      },

      /** @private */
      ipTypes_: {
        type: Array,
        value: [
          IP_TYPES.AUTOMATIC,
          IP_TYPES.IPv4,
          IP_TYPES.IPv6,
          IP_TYPES.IPv4_IPv6,
        ],
        readOnly: true,
      },

      /** @private */
      selectedIPType_: {
        type: String,
        value: IP_TYPES.AUTOMATIC,

      },

      /** @private */
      selectedAuthType_: {
        type: String,
        value: AUTHENTICATION_TYPES.AUTOMATIC,
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
   * @param {string} type
   * @private
   */
  getAuthTypeLocalizedLabel_(type) {
    if (type === AUTHENTICATION_TYPES.AUTOMATIC) {
      return this.i18n('apnDetailTypeAuto');
    } else if (type === AUTHENTICATION_TYPES.CHAP) {
      return this.i18n('apnDetailAuthTypeCHAP');
    } else if (type === AUTHENTICATION_TYPES.PAP) {
      return this.i18n('apnDetailAuthTypePAP');
    }
    console.error('Invalid Authentication type detected');
  }

  /**
   * Returns the localized label for the ip type.
   * @param {string} type
   * @private
   */
  getIPTypeLocalizedLabel_(type) {
    if (type === IP_TYPES.AUTOMATIC) {
      return this.i18n('apnDetailTypeAuto');
    } else if (type === IP_TYPES.IPv4) {
      return this.i18n('apnDetailIPTypeIPV4');
    } else if (type === IP_TYPES.IPv6) {
      return this.i18n('apnDetailIPTypeIPV6');
    } else if (type === IP_TYPES.IPv4_IPv6) {
      return this.i18n('apnDetailIPTypeIPV4_IPV6');
    }
    console.error('Invalid IP type detected');
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
