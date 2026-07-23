// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './checkup_list_item.js';
import './prefs/pref_toggle_button.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_change_details.html.js';
import type {Route} from './router.js';
import {Page, RouteObserverMixin, Router} from './router.js';

export interface PasswordChangeDetailsElement {
  $: {
    back: HTMLElement,
  };
}

const PasswordChangeDetailsElementBase =
    PrefsMixin(I18nMixin(RouteObserverMixin(PolymerElement)));

export class PasswordChangeDetailsElement extends
    PasswordChangeDetailsElementBase {
  static get is() {
    return 'password-change-details';
  }

  static get properties() {
    return {
      isPasswordChangeWithPrivateInferenceLoginCheckEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isPasswordChangeWithPrivateInferenceLoginCheckEnabled');
        },
      },
    };
  }

  static get template() {
    return getTemplate();
  }

  declare protected isPasswordChangeWithPrivateInferenceLoginCheckEnabled_:
      boolean;

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    if (newRoute.page !== Page.PASSWORD_CHANGE ||
        oldRoute?.page === Page.SETTINGS) {
      return;
    }

    setTimeout(() => {
      this.$.back.focus();
    }, 0);
  }

  private navigateBack_() {
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
}

declare global {
  interface HTMLElementTagNameMap {
    'password-change-details': PasswordChangeDetailsElement;
  }
}

customElements.define(
    PasswordChangeDetailsElement.is, PasswordChangeDetailsElement);
