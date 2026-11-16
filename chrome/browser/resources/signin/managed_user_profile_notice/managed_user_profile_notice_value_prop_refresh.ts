// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './managed_user_profile_notice_value_prop_refresh.css.js';
import {getHtml} from './managed_user_profile_notice_value_prop_refresh.html.js';

export interface ManagedUserProfileNoticeValuePropRefreshElement {
  $: {
    title: HTMLElement,
  };
}

export class ManagedUserProfileNoticeValuePropRefreshElement extends
    CrLitElement {
  static get is() {
    return 'managed-user-profile-notice-value-prop-refresh';
  }

  static override get styles() {
    return getCss();
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

  accessor pictureUrl: string = '';
  override accessor title: string = '';
  accessor subtitle: string = '';
  accessor email: string = '';
  accessor accountName: string = '';
  accessor showEnterpriseBadge: boolean = false;

  override firstUpdated() {
    this.$.title.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-value-prop-refresh':
        ManagedUserProfileNoticeValuePropRefreshElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeValuePropRefreshElement.is,
    ManagedUserProfileNoticeValuePropRefreshElement);
