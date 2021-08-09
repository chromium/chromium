// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './icons.js';

import {MouseHoverableMixin, MouseHoverableMixinInterface} from 'chrome://resources/cr_elements/mouse_hoverable_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from './read_later_api_proxy.js';

/** @type {!Set<string>} */
const navigationKeys = new Set([' ', 'Enter', 'ArrowRight', 'ArrowLeft']);

/**
 * @constructor
 * @extends PolymerElement
 * @implements {MouseHoverableMixinInterface}
 * @appliesMixin MouseHoverableMixin
 */
const ReadLaterItemElementBase = MouseHoverableMixin(PolymerElement);

/** @polymer */
export class ReadLaterItemElement extends ReadLaterItemElementBase {
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

      /** @type {boolean} */
      buttonRipples: Boolean,
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
    this.addEventListener('auxclick', this.onClick_);
    this.addEventListener('contextmenu', this.onContextMenu_.bind(this));
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onClick_(e) {
    this.apiProxy_.openURL(this.data.url, true, {
      middleButton: e.type === 'auxclick',
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    });
  }

  /**
   * @param {!Event} e
   * @private
   */
  onContextMenu_(e) {
    this.apiProxy_.showContextMenuForURL(this.data.url, e.clientX, e.clientY);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onKeyDown_(e) {
    if (e.shiftKey || !navigationKeys.has(e.key)) {
      return;
    }
    switch (e.key) {
      case ' ':
      case 'Enter':
        this.onClick_(e);
        break;
      case 'ArrowRight':
        if (!this.shadowRoot.activeElement) {
          this.shadowRoot.getElementById('updateStatusButton').focus();
        } else if (this.shadowRoot.activeElement.nextElementSibling) {
          this.shadowRoot.activeElement.nextElementSibling.focus();
        } else {
          this.focus();
        }
        break;
      case 'ArrowLeft':
        if (!this.shadowRoot.activeElement) {
          this.shadowRoot.getElementById('deleteButton').focus();
        } else if (this.shadowRoot.activeElement.previousElementSibling) {
          this.shadowRoot.activeElement.previousElementSibling.focus();
        } else {
          this.focus();
        }
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
   * @param {string} url
   * @return {string}
   * @private
   */
  getFaviconUrl_(url) {
    return getFaviconForPageURL(url, false);
  }

  /**
   * @param {string} markAsUnreadIcon
   * @param {string} markAsReadIcon
   * @return {string} The appropriate icon for the current state
   * @private
   */
  getUpdateStatusButtonIcon_(markAsUnreadIcon, markAsReadIcon) {
    return this.data.read ? markAsUnreadIcon : markAsReadIcon;
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
