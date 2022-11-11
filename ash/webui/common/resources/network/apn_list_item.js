// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a cellular APN in the APN list.
 */

import './network_shared.css.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './apn_list_item.html.js';

Polymer({
  _template: getTemplate(),
  is: 'apn-list-item',

  behaviors: [I18nBehavior],

  properties: {
    // TODO(b/162365553): Add type annotations for apn property.
    apn: {
      type: Object,
    },

    isConnected: {
      type: Boolean,
      value() {
        return false;
      },
    },

    /**
     * TODO(b/162365553): Implement.
     *  @private
     */
    shouldShowDetailsMenuItem_: {
      type: Boolean,
      value() {
        return true;
      },
    },

    /**
     * TODO(b/162365553): Implement.
     *  @private
     */
    shouldShowDisableMenuItem_: {
      type: Boolean,
      value() {
        return true;
      },
    },

    /**
     * TODO(b/162365553): Implement.
     *  @private
     */
    shouldShowRemoveMenuItem_: {
      type: Boolean,
      value() {
        return true;
      },
    },
  },

  /**
   * Opens the three dots menu.
   * @private
   */
  onMenuButtonClicked_(event) {
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu)
        .showAt(/** @type {!HTMLElement} */ (event.target));
  },

  /**
   * Opens APN Details dialog.
   * TODO(b/162365553): Implement.
   * @private
   */
  onDetailsClicked_() {},

  /**
   * Disables the selected APN.
   * TODO(b/162365553): Implement.
   * @private
   */
  onDisableClicked_() {},

  /**
   * Removes the selected APN.
   * TODO(b/162365553): Implement.
   * @private
   */
  onRemoveClicked_() {},
});