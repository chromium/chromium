// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '/strings.m.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './managed_user_profile_notice_disclosure.css.js';
import {getHtml} from './managed_user_profile_notice_disclosure.html.js';

const ManagedUserProfileNoticeDisclosureElementBase =
    I18nMixinLit(CrLitElement);

export class ManagedUserProfileNoticeDisclosureElement extends
    ManagedUserProfileNoticeDisclosureElementBase {
  static get is() {
    return 'managed-user-profile-notice-disclosure';
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
      isOidcDialog_: {type: Boolean},
      pictureUrl: {type: String},
      disclosureTitle_: {type: String},
    };
  }

  accessor showEnterpriseBadge: boolean = false;
  accessor pictureUrl: string = '';

  protected accessor isOidcDialog_: boolean =
      loadTimeData.getBoolean('isOidcDialog');
  protected accessor disclosureTitle_: string = '';

  override firstUpdated() {
    const titleElement = this.shadowRoot.querySelector<HTMLElement>('.title');
    assert(titleElement);
    titleElement.focus();
  }

  protected computeDisclosureTitle_() {
    return this.i18n(
        this.isOidcDialog_ ? 'profileOidcDisclosureTitle' :
                             'profileDisclosureTitle');
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
