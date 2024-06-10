// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './strings.m.js';
import './signin_shared.css.js';
import './signin_vars.css.js';
import './tangible_sync_style_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './managed_user_profile_notice_disclosure.html.js';

const ManagedUserProfileNoticeDisclosureElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class ManagedUserProfileNoticeDisclosureElement extends
    ManagedUserProfileNoticeDisclosureElementBase {
  static get is() {
    return 'managed-user-profile-notice-disclosure';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showEnterpriseBadge: Boolean,
      pictureUrl: String,
      title: String,
      subtitle: String,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-disclosure':
        ManagedUserProfileNoticeDisclosureElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeDisclosureElement.is,
    ManagedUserProfileNoticeDisclosureElement);
