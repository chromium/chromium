// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';
import './checkup_list_item.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_change_details.html.js';
import {Page, Router} from './router.js';

export interface PasswordChangeDetailsElement {
  $: {
    back: HTMLElement,
  };
}

const PasswordChangeDetailsElementBase = I18nMixin(PolymerElement);

export class PasswordChangeDetailsElement extends
    PasswordChangeDetailsElementBase {
  static get is() {
    return 'password-change-details';
  }

  static get template() {
    return getTemplate();
  }

  private navigateBack_() {
    Router.getInstance().navigateTo(Page.SETTINGS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-change-details': PasswordChangeDetailsElement;
  }
}

customElements.define(
    PasswordChangeDetailsElement.is, PasswordChangeDetailsElement);
