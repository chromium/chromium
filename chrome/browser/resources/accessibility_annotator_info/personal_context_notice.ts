// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '/strings.m.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {PersonalContextNoticeBrowserProxy} from './browser_proxy.js';
import {getCss} from './personal_context_notice.css.js';
import {getHtml} from './personal_context_notice.html.js';

const PersonalContextNoticeElementBase = I18nMixinLit(CrLitElement);

export class PersonalContextNoticeElement extends
    PersonalContextNoticeElementBase {
  static get is() {
    return 'personal-context-notice';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      email_: {type: String},
      avatarUrl_: {type: String},
    };
  }

  protected accessor email_: string = '';
  protected accessor avatarUrl_: string = '';

  override connectedCallback() {
    super.connectedCallback();
    PersonalContextNoticeBrowserProxy.getInstance()
        .handler.getAccountInfo()
        .then(response => {
          if (response.info) {
            this.email_ = response.info.email;
            this.avatarUrl_ = response.info.avatarUrl;
          }
          PersonalContextNoticeBrowserProxy.getInstance().handler.showUi();
        });
  }

  protected onManageSettingsClick_() {
    PersonalContextNoticeBrowserProxy.getInstance()
        .handler.onManageSettingsClicked();
  }

  protected onLearnMoreClick_(e: Event) {
    const target = e.target as HTMLElement;
    if (target.tagName === 'A') {
      e.preventDefault();
      PersonalContextNoticeBrowserProxy.getInstance()
          .handler.onLearnMoreClicked();
    }
  }

  protected onGotItClick_() {
    PersonalContextNoticeBrowserProxy.getInstance()
        .handler.onInfoAcknowledged();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'personal-context-notice': PersonalContextNoticeElement;
  }
}

customElements.define(
    PersonalContextNoticeElement.is, PersonalContextNoticeElement);
