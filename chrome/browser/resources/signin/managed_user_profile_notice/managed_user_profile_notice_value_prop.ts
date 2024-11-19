// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './managed_user_profile_notice_value_prop.css.js';
import {getHtml} from './managed_user_profile_notice_value_prop.html.js';

export class ManagedUserProfileNoticeValuePropElement extends CrLitElement {
  static get is() {
    return 'managed-user-profile-notice-value-prop';
  }

  static override get styles() {
    return getCss();
  }

  get titleElement(): HTMLElement|undefined {
    return this.shadowRoot?.querySelector('.title') || undefined;
  }

  override firstUpdated() {
    this.titleElement?.focus();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pictureUrl: {type: String},
      title: {type: String},
      subtitle: {type: String},
      email: {type: String},
      accountName: {type: String},
      showEnterpriseBadge: {type: Boolean},
    };
  }

  pictureUrl: string = '';
  override title: string = '';
  subtitle: string = '';
  email: string = '';
  accountName: string = '';
  showEnterpriseBadge: boolean = false;
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-value-prop':
        ManagedUserProfileNoticeValuePropElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeValuePropElement.is,
    ManagedUserProfileNoticeValuePropElement);
