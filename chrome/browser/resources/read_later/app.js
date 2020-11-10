// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from './read_later_api_proxy.js';
import {ReadLaterItemElement} from './read_later_item.js';

/** @type {!Set<string>} */
const navigationKeys = new Set(['ArrowDown', 'ArrowUp']);

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
    // Push ShowUI() callback to the event queue to allow deferred rendering to
    // take place.
    // TODO(corising): Determine the ideal place to make this call.
    setTimeout(() => {
      this.apiProxy_.showUI();
    }, 0);
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

  /**
   * @param {!Event} e
   * @private
   */
  onItemKeyDown_(e) {
    if (e.shiftKey || !navigationKeys.has(e.key)) {
      return;
    }
    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
    switch (e.key) {
      case 'ArrowDown':
        selector.selectNext();
        /** @type {!ReadLaterItemElement} */ (selector.selectedItem).focus();
        break;
      case 'ArrowUp':
        selector.selectPrevious();
        /** @type {!ReadLaterItemElement} */ (selector.selectedItem).focus();
        break;
      default:
        assertNotReached();
        return;
    }
    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemFocus_(e) {
    this.$.selector.selected =
        /** @type {!ReadLaterItemElement} */ (e.currentTarget).dataset.url;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onCloseClick_(e) {
    e.stopPropagation();
    this.apiProxy_.closeUI();
  }
}

customElements.define(ReadLaterAppElement.is, ReadLaterAppElement);
