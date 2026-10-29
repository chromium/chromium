// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './share_password_group_avatar.css.js';
import {getHtml} from './share_password_group_avatar.html.js';

export interface SharePasswordGroupAvatarElement {
  $: {
    firstImg: HTMLImageElement,
    secondImg: HTMLImageElement,
    thirdImg: HTMLImageElement,
    fourthImg: HTMLImageElement,
    more: HTMLElement,
  };
}

export class SharePasswordGroupAvatarElement extends CrLitElement {
  static get is() {
    return 'share-password-group-avatar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      members: {type: Array},
    };
  }

  accessor members: chrome.passwordsPrivate.RecipientInfo[] = [];

  protected isMembersCountLessThan_(count: number): boolean {
    return this.members.length < count;
  }

  protected getProfileImageUrl_(index: number): string {
    if (index < this.members.length) {
      return this.members[index].profileImageUrl;
    }
    return '';
  }

  protected getMoreCount_(): number {
    return this.members.length - 3;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'share-password-group-avatar': SharePasswordGroupAvatarElement;
  }
}

customElements.define(
    SharePasswordGroupAvatarElement.is, SharePasswordGroupAvatarElement);
