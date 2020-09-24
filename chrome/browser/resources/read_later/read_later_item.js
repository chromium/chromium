// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from './read_later_api_proxy.js';

export class ReadLaterItemElement extends PolymerElement {
  static get is() {
    return 'read-later-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!readLater.mojom.ReadLaterEntry} */
      data: Object,
    };
  }

  constructor() {
    super();
    /** @private {!ReadLaterApiProxy} */
    this.apiProxy_ = ReadLaterApiProxyImpl.getInstance();
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener('click', this.onClick_);
  }

  /** @private */
  onClick_() {
    this.apiProxy_.openSavedEntry(this.data.url);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onUpdateStatusClick_(e) {
    e.stopPropagation();
    this.apiProxy_.updateReadStatus(this.data.url, !this.data.read);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemDeleteClick_(e) {
    e.stopPropagation();
    this.apiProxy_.removeEntry(this.data.url);
  }

  /**
   * @param {string} markAsUnreadTooltip
   * @param {string} markAsReadTooltip
   * @return {string} The appropriate tooltip for the current state
   * @private
   */
  getUpdateStatusButtonTooltip_(markAsUnreadTooltip, markAsReadTooltip) {
    return this.data.read ? markAsUnreadTooltip : markAsReadTooltip;
  }
}

customElements.define(ReadLaterItemElement.is, ReadLaterItemElement);
