// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyPresenceBrowserProxy} from './nearby_presence_browser_proxy.js';
import {getTemplate} from './np_list_object.html.js';
import type {PresenceDevice} from './types.js';


/** @polymer */
class NpObjectElement extends PolymerElement {
  static get is() {
    return 'np-object';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      device: {
        type: Object,
      },
    };
  }
  device: PresenceDevice;
  private browserProxy_: NearbyPresenceBrowserProxy =
      NearbyPresenceBrowserProxy.getInstance();

  private onConnectClicked_(): void {
    this.browserProxy_.connectToPresenceDevice(this.device.endpoint_id);
  }
}

customElements.define(NpObjectElement.is, NpObjectElement);
