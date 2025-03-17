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

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_proxy_exclusions.html.js';

const NetworkProxyExclusionsElementBase = I18nMixin(PolymerElement);

export class NetworkProxyExclusionsElement extends
    NetworkProxyExclusionsElementBase {
  static get is() {
    return 'network-proxy-exclusions' as const;
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

  editable: boolean;
  exclusions: string[];

  private onRemoveTap_(event: {model: {index: number}}): void {
    const index = event.model.index;
    this.splice('exclusions', index, 1);
    this.dispatchEvent(new CustomEvent(
        'proxy-exclusions-change', {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkProxyExclusionsElement.is]: NetworkProxyExclusionsElement;
  }
}

customElements.define(
    NetworkProxyExclusionsElement.is, NetworkProxyExclusionsElement);
