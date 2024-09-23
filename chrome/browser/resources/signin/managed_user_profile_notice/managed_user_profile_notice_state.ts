// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './managed_user_profile_notice_state.css.js';
import {getHtml} from './managed_user_profile_notice_state.html.js';

export class ManagedUserProfileNoticeStateElement extends CrLitElement {
  static get is() {
    return 'managed-user-profile-notice-state';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      title: {type: String},
      subtitle: {type: String},
    };
  }

  override title: string;
  subtitle: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-state': ManagedUserProfileNoticeStateElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeStateElement.is,
    ManagedUserProfileNoticeStateElement);
