// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-data-usage-dialog' allows editing of the data usage setting
 * when using Nearby Share.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNearbyShareSettings} from '../../shared/nearby_share_settings.js';

import {dataUsageStringToEnum, NearbyShareDataUsage} from './types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NearbyShareDataUsageDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class NearbyShareDataUsageDialogElement extends
    NearbyShareDataUsageDialogElementBase {
  static get is() {
    return 'nearby-share-data-usage-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Object<string, number>} */
      NearbyShareDataUsage: {
        type: Object,
        value: NearbyShareDataUsage,
      },
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (!dialog.open) {
      dialog.showModal();
    }
  }

  /** @private */
  close() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  }

  /** @private */
  onCancelClick_() {
    this.close();
  }

  /** @private */
  onSaveClick_() {
    getNearbyShareSettings().setDataUsage((dataUsageStringToEnum(
        this.shadowRoot.querySelector('cr-radio-group').selected)));
    this.close();
  }

  /** @private */
  selectedDataUsage_(dataUsageValue) {
    if (dataUsageValue === NearbyShareDataUsage.UNKNOWN) {
      return NearbyShareDataUsage.WIFI_ONLY;
    }

    return dataUsageValue;
  }
}

customElements.define(
    NearbyShareDataUsageDialogElement.is, NearbyShareDataUsageDialogElement);
