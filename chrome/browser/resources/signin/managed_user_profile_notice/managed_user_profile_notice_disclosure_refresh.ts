// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './managed_user_profile_notice_disclosure_refresh.css.js';
import {getHtml} from './managed_user_profile_notice_disclosure_refresh.html.js';

export interface ManagedUserProfileNoticeDisclosureRefreshElement {
  $: {
    avatar: HTMLImageElement,
    title: HTMLElement,
    subtitle: HTMLElement,
  };
}

const ManagedUserProfileNoticeDisclosureRefreshElementBase =
    I18nMixinLit(CrLitElement);

export class ManagedUserProfileNoticeDisclosureRefreshElement extends
    ManagedUserProfileNoticeDisclosureRefreshElementBase {
  static get is() {
    return 'managed-user-profile-notice-disclosure-refresh';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showEnterpriseBadge: {type: Boolean},
      pictureUrl: {type: String},
      title: {type: String},
      subtitle: {type: String},
    };
  }

  accessor showEnterpriseBadge: boolean = false;
  accessor pictureUrl: string = '';
  override accessor title: string = '';
  accessor subtitle: string = '';

  override firstUpdated() {
    const titleElement = this.shadowRoot.querySelector<HTMLElement>('.title');
    assert(titleElement);
    titleElement.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-disclosure-refresh':
        ManagedUserProfileNoticeDisclosureRefreshElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeDisclosureRefreshElement.is,
    ManagedUserProfileNoticeDisclosureRefreshElement);
