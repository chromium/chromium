// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './checkup_list_item.js';
import './prefs/pref_toggle_button.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './password_change_details.css.js';
import {getHtml} from './password_change_details.html.js';
import type {Route} from './router.js';
import {Page, RouteObserverMixinLit, Router} from './router.js';

export interface PasswordChangeDetailsElement {
  $: {
    back: HTMLElement,
  };
}

const PasswordChangeDetailsElementBase =
    I18nMixinLit(RouteObserverMixinLit(CrLitElement));

export class PasswordChangeDetailsElement extends
    PasswordChangeDetailsElementBase {
  static get is() {
    return 'password-change-details';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isPasswordChangeWithPrivateInferenceLoginCheckEnabled_: {
        type: Boolean,
      },
    };
  }

  protected accessor isPasswordChangeWithPrivateInferenceLoginCheckEnabled_:
      boolean = loadTimeData.getBoolean(
          'isPasswordChangeWithPrivateInferenceLoginCheckEnabled');

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    if (newRoute.page !== Page.PASSWORD_CHANGE ||
        oldRoute?.page === Page.SETTINGS) {
      return;
    }

    setTimeout(() => {
      this.$.back.focus();
    }, 0);
  }

  protected onBackClick_() {
    Router.getInstance().navigateTo(Page.SETTINGS);
  }

  protected getAccountBoxIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:account-box' :
        'passwords-icon:account-box-old';
  }

  protected getChatInfoIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:chat-info' :
        'passwords-icon:chat-info-old';
  }

  protected getLockIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:lock' :
        'passwords-icon:lock-old';
  }

  protected getPsychiatryIcon_(): string {
    return loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
        'passwords-icon:psychiatry' :
        'passwords-icon:psychiatry-old';
  }

  protected getColumnDescriptionHeader_(): string {
    return this.i18n(
        this.isPasswordChangeWithPrivateInferenceLoginCheckEnabled_ ?
            'columnHeadingWhenOn' :
            'columnHeadingWhenUsed');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-change-details': PasswordChangeDetailsElement;
  }
}

customElements.define(
    PasswordChangeDetailsElement.is, PasswordChangeDetailsElement);
