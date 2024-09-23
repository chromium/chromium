// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and editing a single
 * network proxy value. When the URL or port changes, a 'proxy-input-change'
 * event is fired with the combined url and port values passed as a single
 * string, url:port.
 */

import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import './network_shared.css.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {ManagedProxyLocation} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_proxy_input.html.js';
import {OncMojo} from './onc_mojo.js';

Polymer({
  _template: getTemplate(),
  is: 'network-proxy-input',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Whether or not the proxy value can be edited.
     */
    editable: {
      type: Boolean,
      value: false,
    },

    /**
     * A label for the proxy value.
     */
    label: {
      type: String,
      value: 'Proxy',
    },

    /**
     * The proxy object.
     * @type {!ManagedProxyLocation}
     */
    value: {
      type: Object,
      value() {
        return {
          host: OncMojo.createManagedString(''),
          port: OncMojo.createManagedInt(80),
        };
      },
      notify: true,
    },
  },

  focus() {
    this.$$('cr-input').focus();
  },

  /**
   * Event triggered when an input value changes.
   * @private
   */
  onValueChange_() {
    let port = parseInt(this.value.port.activeValue, 10);
    if (isNaN(port)) {
      port = 80;
    }
    this.value.port.activeValue = port;
    this.fire('proxy-input-change', this.value);
  },
});
