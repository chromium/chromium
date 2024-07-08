// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './strings.m.js';
import './tangible_sync_style_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './managed_user_profile_notice_state.html.js';

export class ManagedUserProfileNoticeStateElement extends PolymerElement {
  static get is() {
    return 'managed-user-profile-notice-state';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      icon: String,
      title: String,
      subtitle: String,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-state': ManagedUserProfileNoticeStateElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeStateElement.is,
    ManagedUserProfileNoticeStateElement);
