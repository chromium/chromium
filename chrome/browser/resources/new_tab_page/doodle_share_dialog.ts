// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './doodle_share_dialog.html.js';
import {DoodleShareChannel} from './new_tab_page.mojom-webui.js';
import {WindowProxy} from './window_proxy.js';

/**
 * The ID of the doodle app for Facebook. Used to share doodles to Facebook.
 */
const FACEBOOK_APP_ID: number = 738026486351791;

export interface DoodleShareDialogElement {
  $: {
    dialog: CrDialogElement,
    copyButton: HTMLElement,
    doneButton: HTMLElement,
    emailButton: HTMLElement,
    title: HTMLElement,
    url: CrInputElement,
  };
}

/** Dialog that lets the user share the doodle. */
export class DoodleShareDialogElement extends PolymerElement {
  static get is() {
    return 'ntp-doodle-share-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Title shown on the dialog. */
      title: String,

      /** Share URL provided to the user. */
      url: Object,
    };
  }

  override title: string;
  url: Url;

  private onFacebookClick_() {
    const url = 'https://www.facebook.com/dialog/share' +
        `?app_id=${FACEBOOK_APP_ID}` +
        `&href=${encodeURIComponent(this.url.url)}` +
        `&hashtag=${encodeURIComponent('#GoogleDoodle')}`;
    WindowProxy.getInstance().open(url);
    this.notifyShare_(DoodleShareChannel.kFacebook);
  }

  private onTwitterClick_() {
    const url = 'https://twitter.com/intent/tweet' +
        `?text=${encodeURIComponent(`${this.title}\n${this.url.url}`)}`;
    WindowProxy.getInstance().open(url);
    this.notifyShare_(DoodleShareChannel.kTwitter);
  }

  private onEmailClick_() {
    const url = `mailto:?subject=${encodeURIComponent(this.title)}` +
        `&body=${encodeURIComponent(this.url.url)}`;
    WindowProxy.getInstance().navigate(url);
    this.notifyShare_(DoodleShareChannel.kEmail);
  }

  private onCopyClick_() {
    this.$.url.select();
    navigator.clipboard.writeText(this.url.url);
    this.notifyShare_(DoodleShareChannel.kLinkCopy);
  }

  private onCloseClick_() {
    this.$.dialog.close();
  }

  private notifyShare_(channel: DoodleShareChannel) {
    this.dispatchEvent(new CustomEvent('share', {detail: channel}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-doodle-share-dialog': DoodleShareDialogElement;
  }
}

customElements.define(DoodleShareDialogElement.is, DoodleShareDialogElement);
