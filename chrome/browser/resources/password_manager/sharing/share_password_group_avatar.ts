// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './share_password_group_avatar.html.js';

export interface SharePasswordGroupAvatarElement {
  $: {
    firstImg: HTMLImageElement,
    secondImg: HTMLImageElement,
    thirdImg: HTMLImageElement,
    fourthImg: HTMLImageElement,
    more: HTMLElement,
  };
}
export class SharePasswordGroupAvatarElement extends PolymerElement {
  static get is() {
    return 'share-password-group-avatar';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      members: Array,
    };
  }

  members: chrome.passwordsPrivate.RecipientInfo[];

  private isMembersCountLessThan_(count: number): boolean {
    return this.members.length < count;
  }

  private getProfileImageUrl_(index: number): string {
    if (index < this.members.length) {
      return this.members[index].profileImageUrl;
    }
    return '';
  }

  private getMoreCount_(): number {
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
