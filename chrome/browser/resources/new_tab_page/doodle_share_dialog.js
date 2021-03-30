// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {WindowProxy} from './window_proxy.js';

/**
 * The ID of the doodle app for Facebook. Used to share doodles to Facebook.
 * @type {number}
 */
const FACEBOOK_APP_ID = 738026486351791;

// Dialog that lets the user share the doodle.
class DoodleShareDialogElement extends PolymerElement {
  static get is() {
    return 'ntp-doodle-share-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Title shown on the dialog.
       * @type {string}
       */
      title: String,

      /**
       * Share URL provided to the user.
       * @type {url.mojom.Url}
       */
      url: Object,
    };
  }

  /** @private */
  onFacebookClick_() {
    const url = 'https://www.facebook.com/dialog/share' +
        `?app_id=${FACEBOOK_APP_ID}` +
        `&href=${encodeURIComponent(this.url.url)}` +
        `&hashtag=${encodeURIComponent('#GoogleDoodle')}`;
    WindowProxy.getInstance().open(url);
    this.notifyShare_(newTabPage.mojom.DoodleShareChannel.kFacebook);
  }

  /** @private */
  onTwitterClick_() {
    const url = 'https://twitter.com/intent/tweet' +
        `?text=${encodeURIComponent(`${this.title}\n${this.url.url}`)}`;
    WindowProxy.getInstance().open(url);
    this.notifyShare_(newTabPage.mojom.DoodleShareChannel.kTwitter);
  }

  /** @private */
  onEmailClick_() {
    const url = `mailto:?subject=${encodeURIComponent(this.title)}` +
        `&body=${encodeURIComponent(this.url.url)}`;
    WindowProxy.getInstance().navigate(url);
    this.notifyShare_(newTabPage.mojom.DoodleShareChannel.kEmail);
  }

  /** @private */
  onCopyClick_() {
    this.$.url.select();
    navigator.clipboard.writeText(this.url.url);
    this.notifyShare_(newTabPage.mojom.DoodleShareChannel.kLinkCopy);
  }

  /** @private */
  onCloseClick_() {
    this.$.dialog.close();
  }

  /**
   * @param {newTabPage.mojom.DoodleShareChannel} channel
   * @private
   */
  notifyShare_(channel) {
    this.dispatchEvent(new CustomEvent('share', {detail: channel}));
  }
}

customElements.define(DoodleShareDialogElement.is, DoodleShareDialogElement);
