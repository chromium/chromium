// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of proxy exclusions.
 * Includes UI for adding, changing, and removing entries.
 */

import '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/ash/common/cr_elements/cr_hidden_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './network_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_proxy_exclusions.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NetworkProxyExclusionsElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class NetworkProxyExclusionsElement extends NetworkProxyExclusionsElementBase {
  static get is() {
    return 'network-proxy-exclusions';
  }
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Whether or not the proxy values can be edited. */
      editable: {
        type: Boolean,
        value: false,
      },

      /**
       * The list of exclusions.
       * @type {!Array<string>}
       */
      exclusions: {
        type: Array,
        value() {
          return [];
        },
        notify: true,
      },
    };
  }

  /**
   * Event triggered when an item is removed.
   * @param {!{model: !{index: number}}} event
   * @private
   */
  onRemoveTap_(event) {
    const index = event.model.index;
    this.splice('exclusions', index, 1);
    this.dispatchEvent(new CustomEvent(
        'proxy-exclusions-change', {bubbles: true, composed: true}));
  }
}

customElements.define(
    NetworkProxyExclusionsElement.is, NetworkProxyExclusionsElement);
