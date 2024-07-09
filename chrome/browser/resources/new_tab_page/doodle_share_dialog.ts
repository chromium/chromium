// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './doodle_share_dialog.css.js';
import {getHtml} from './doodle_share_dialog.html.js';
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
export class DoodleShareDialogElement extends CrLitElement {
  static get is() {
    return 'ntp-doodle-share-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** Title shown on the dialog. */
      title: {type: String},

      /** Share URL provided to the user. */
      url: {type: Object},
    };
  }

  override title: string;
  url: Url = {url: ''};

  protected onFacebookClick_() {
    const url = 'https://www.facebook.com/dialog/share' +
        `?app_id=${FACEBOOK_APP_ID}` +
        `&href=${encodeURIComponent(this.url.url)}` +
        `&hashtag=${encodeURIComponent('#GoogleDoodle')}`;
    WindowProxy.getInstance().open(url);
    this.notifyShare_(DoodleShareChannel.kFacebook);
  }

  protected onTwitterClick_() {
    const url = 'https://twitter.com/intent/tweet' +
        `?text=${encodeURIComponent(`${this.title}\n${this.url.url}`)}`;
    WindowProxy.getInstance().open(url);
    this.notifyShare_(DoodleShareChannel.kTwitter);
  }

  protected onEmailClick_() {
    const url = `mailto:?subject=${encodeURIComponent(this.title)}` +
        `&body=${encodeURIComponent(this.url.url)}`;
    WindowProxy.getInstance().navigate(url);
    this.notifyShare_(DoodleShareChannel.kEmail);
  }

  protected onCopyClick_() {
    this.$.url.select();
    navigator.clipboard.writeText(this.url.url);
    this.notifyShare_(DoodleShareChannel.kLinkCopy);
  }

  protected onCloseClick_() {
    this.$.dialog.close();
  }

  private notifyShare_(channel: DoodleShareChannel) {
    this.fire('share', channel);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-doodle-share-dialog': DoodleShareDialogElement;
  }
}

customElements.define(DoodleShareDialogElement.is, DoodleShareDialogElement);
