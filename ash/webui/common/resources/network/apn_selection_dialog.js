// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Dialog that displays APNs for a user to select to be attempted to be used.
 */

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/network/apn_selection_dialog_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {assert} from '//resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {ApnProperties, CrosNetworkConfigInterface} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './apn_selection_dialog.html.js';
import {MojoInterfaceProviderImpl} from './mojo_interface_provider.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ApnSelectionDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ApnSelectionDialog extends ApnSelectionDialogElementBase {
  static get is() {
    return 'apn-selection-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {Array<ApnProperties>} */
      apnList: {
        type: Array,
        value: [],
      },

      /** The GUID of the network to select known APNs for. */
      guid: String,

      /** @type {ApnProperties} */
      selectedApn_: {
        type: Object,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!CrosNetworkConfigInterface} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onCancelClicked_(event) {
    event.stopPropagation();
    if (this.$.apnSelectionDialog.open) {
      this.$.apnSelectionDialog.close();
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onActionButtonClicked_(event) {
    assert(this.guid);

    if (!this.selectedApn_) {
      return;
    }

    this.networkConfig_.createExclusivelyEnabledCustomApn(
        this.guid, this.selectedApn_);
    this.$.apnSelectionDialog.close();
  }

  /**
   * @param {!ApnProperties} apn
   * @return {boolean}
   * @private
   */
  isApnSelected_(apn) {
    return apn === this.selectedApn_;
  }
}

customElements.define(ApnSelectionDialog.is, ApnSelectionDialog);
