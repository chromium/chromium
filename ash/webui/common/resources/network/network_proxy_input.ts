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

import {assert} from '//resources/js/assert.js';
import type {ManagedProxyLocation} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {getTemplate} from './network_proxy_input.html.js';
import {OncMojo} from './onc_mojo.js';

const NetworkProxyInputElementBase = I18nMixin(PolymerElement);

export class NetworkProxyInputElement extends NetworkProxyInputElementBase {
  static get is() {
    return 'network-proxy-input' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
    };
  }

  editable: boolean;
  label: string;
  value: ManagedProxyLocation;

  override focus(): void {
    const crInput = this.shadowRoot!.querySelector('cr-input');
    assert(!!crInput);
    crInput.focus();
  }

  /**
   * Event triggered when an input value changes.
   */
  private onValueChange_(): void {
    let port = parseInt(this.value.port.activeValue.toString(), 10);
    if (isNaN(port)) {
      port = 80;
    }
    this.value.port.activeValue = port;
    this.dispatchEvent(new CustomEvent(
        'proxy-input-change',
        {bubbles: true, composed: true, detail: this.value}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkProxyInputElement.is]: NetworkProxyInputElement;
  }
}

customElements.define(NetworkProxyInputElement.is, NetworkProxyInputElement);
