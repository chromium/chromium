// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of cellular
 * APNs
 */

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './network_shared.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/ash/common/network/apn_list_item.js';
import 'chrome://resources/ash/common/network/apn_detail_dialog.js';

import {assert} from '//resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {ApnDetailDialog} from '//resources/ash/common/network/apn_detail_dialog.js';
import {afterNextRender, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ApnDetailDialogMode, ApnEventData} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnProperties, ApnState, ApnType, ManagedCellularProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';

import {getTemplate} from './apn_list.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ApnListBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ApnList extends ApnListBase {
  static get is() {
    return 'apn-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** The GUID of the network to display details for. */
      guid: String,

      /**@type {!ManagedCellularProperties}*/
      managedCellularProperties: {
        type: Object,
      },

      shouldOmitLinks: {
        type: Boolean,
        value: false,
      },

      /** @private */
      shouldShowApnDetailDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * The mode in which the apn detail dialog is opened.
       * @type {ApnDetailDialogMode}
       * @private
       */
      apnDetailDialogMode_: {
        type: Object,
        value: ApnDetailDialogMode.CREATE,
      },
    };
  }

  openApnDetailDialogInCreateMode() {
    this.showApnDetailDialog_(ApnDetailDialogMode.CREATE, /* apn= */ undefined);
  }

  /**
   * Returns an array with all the APN properties that need to be displayed.
   * TODO(b/162365553): Handle managedCellularProperties.apnList.policyValue
   * when policies are included.
   * @return {Array<!ApnProperties>}
   * @private
   */
  getApns_() {
    if (!this.managedCellularProperties) {
      return [];
    }

    const connectedApn = this.managedCellularProperties.connectedApn;
    const customApnList = this.managedCellularProperties.customApnList;

    if (!connectedApn) {
      // TODO(b/162365553): Show error when there is no connected APN.
      return customApnList || [];
    }

    if (!customApnList || customApnList.length === 0) {
      return [connectedApn];
    }

    const connectedApnIndex = customApnList.findIndex(
        (apn) => OncMojo.apnMatch(
            /** @type {!ApnProperties} */ (apn),
            /** @type {!ApnProperties} */ (connectedApn)));

    if (connectedApnIndex != -1) {
      customApnList.splice(connectedApnIndex, 1);
    }

    return [connectedApn, ...customApnList];
  }

  /**
   * Returns true if the APN on this index is connected.
   * @param {number} index index in the APNs array.
   * @return {boolean}
   * @private
   */
  isApnConnected_(index) {
    return !!this.managedCellularProperties &&
        !!this.managedCellularProperties.connectedApn && index === 0;
  }

  /**
   * Returns true if currentApn is the only enabled default APN and there is
   * at least one enabled attach APN.
   * @param {!ApnProperties} currentApn
   * @return {boolean}
   * @private
   */
  shouldDisallowDisablingRemoving_(currentApn) {
    assert(this.managedCellularProperties);
    if (!currentApn.id) {
      return true;
    }

    const customApnList = this.managedCellularProperties.customApnList;
    if (!customApnList) {
      return false;
    }

    if (!customApnList.some(
            apn => !!apn.apnTypes && apn.apnTypes.includes(ApnType.kAttach) &&
                !apn.apnTypes.includes(ApnType.kDefault) &&
                apn.state === ApnState.kEnabled)) {
      return false;
    }

    const defaultEnabledApnList = customApnList.filter(
        apn => !!apn.apnTypes && apn.apnTypes.includes(ApnType.kDefault) &&
            apn.state === ApnState.kEnabled);

    return defaultEnabledApnList.length === 1 &&
        currentApn.id === defaultEnabledApnList[0].id;
  }

  /**
   * Returns true if there are no enabled default APNs and the current APN has
   * only an attach APN type.
   * @param {!ApnProperties} currentApn
   * @return {boolean}
   * @private
   */
  shouldDisallowEnabling_(currentApn) {
    assert(this.managedCellularProperties);
    if (!currentApn.id) {
      return true;
    }

    const customApnList = this.managedCellularProperties.customApnList;
    if (!customApnList) {
      return false;
    }

    if (customApnList.some(
            apn => !!apn.apnTypes && apn.apnTypes.includes(ApnType.kDefault) &&
                apn.state === ApnState.kEnabled)) {
      return false;
    }

    return !!currentApn.apnTypes &&
        currentApn.apnTypes.includes(ApnType.kAttach) &&
        !currentApn.apnTypes.includes(ApnType.kDefault);
  }

  /**
   * Redirects to "Lean more about APN" page.
   * TODO(b/162365553): Implement.
   * @private
   */
  onLearnMoreClicked_() {}

  /**
   * @param {!Event} event
   * @private
   */
  onShowApnDetailDialog_(event) {
    event.stopPropagation();
    if (this.shouldShowApnDetailDialog_) {
      return;
    }
    const eventData = /** @type {!ApnEventData} */ (event.detail);
    this.showApnDetailDialog_(eventData.mode, eventData.apn);
  }

  /**
   * @param {!ApnDetailDialogMode} mode
   * @param {ApnProperties|undefined} apn
   * @private
   */
  showApnDetailDialog_(mode, apn) {
    this.shouldShowApnDetailDialog_ = true;
    this.apnDetailDialogMode_ = mode;
    // Added to ensure dom-if stamping.
    afterNextRender(this, () => {
      const apnDetailDialog = /** @type {ApnDetailDialog} */ (
          this.shadowRoot.querySelector('#apnDetailDialog'));
      assert(!!apnDetailDialog);
      apnDetailDialog.apnProperties = apn;
    });
  }

  /**
   *
   * @param event {!Event}
   * @private
   */
  onApnDetailDialogClose_(event) {
    this.shouldShowApnDetailDialog_ = false;
  }

  /**
   * @returns {Array<ApnProperties>}
   * @private
   */
  getCustomApns_() {
    return this.managedCellularProperties.customApnList ?? [];
  }
}

customElements.define(ApnList.is, ApnList);