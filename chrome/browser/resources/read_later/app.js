// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from './read_later_api_proxy.js';
import {ReadLaterItemElement} from './read_later_item.js';

export class ReadLaterAppElement extends PolymerElement {
  static get is() {
    return 'read-later-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {?Array<!readLater.mojom.ReadLaterEntry>} */
      unreadItems_: Array,

      /** @private {?Array<!readLater.mojom.ReadLaterEntry>} */
      readItems_: Array,
    };
  }

  constructor() {
    super();
    /** @private {!ReadLaterApiProxy} */
    this.apiProxy_ = ReadLaterApiProxyImpl.getInstance();

    /** @private {?number} */
    this.listenerId_ = null;
  }

  /** @override */
  ready() {
    super.ready();
    this.updateItems_();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerId_ =
        callbackRouter.itemsChanged.addListener(() => this.updateItems_());
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.apiProxy_.getCallbackRouter().removeListener(
        /** @type {number} */ (this.listenerId_));
    this.listenerId_ = null;
  }

  /** @private */
  updateItems_() {
    this.apiProxy_.getReadLaterEntries().then(({entries}) => {
      this.unreadItems_ = entries.unreadEntries;
      this.readItems_ = entries.readEntries;
    });
  }
}

customElements.define(ReadLaterAppElement.is, ReadLaterAppElement);
