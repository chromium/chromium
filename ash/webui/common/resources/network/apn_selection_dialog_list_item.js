// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * List item that displays an APN a user may select to attempt to be used.
 */

import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {getApnDisplayName} from '//resources/ash/common/network/cellular_utils.js';
import {ApnProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './apn_selection_dialog_list_item.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ApnSelectionDialogListItemElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ApnSelectionDialogListItem extends
    ApnSelectionDialogListItemElementBase {
  static get is() {
    return 'apn-selection-dialog-list-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {ApnProperties} */
      apn: {
        type: Object,
      },

      selected: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  /**
   * @param {!ApnProperties} apn
   * @return {string}
   * @private
   */
  getApnDisplayName_(apn) {
    return getApnDisplayName(this.i18n.bind(this), apn);
  }

  /**
   * @param {!ApnProperties} apn
   * @return {boolean}
   * @private
   */
  shouldHideSecondaryApnName_(apn) {
    return apn.accessPointName === this.getApnDisplayName_(apn);
  }
}

customElements.define(
    ApnSelectionDialogListItem.is, ApnSelectionDialogListItem);
