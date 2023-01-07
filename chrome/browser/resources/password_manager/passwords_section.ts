// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './strings.m.js';
import './password_list_item.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './passwords_section.html.js';

export interface PasswordsSectionElement {
  $: {
    passwordsList: IronListElement,
  };
}

export class PasswordsSectionElement extends PolymerElement {
  static get is() {
    return 'passwords-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Passwords displayed in the device-only subsection.
       */
      passwords_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private passwords_: chrome.passwordsPrivate.PasswordUiEntry[] = [];

  private setSavedPasswordsListener_: (
      (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void)|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.setSavedPasswordsListener_ = passwordList => {
      this.passwords_ = passwordList;
    };

    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        passwords => this.passwords_ = passwords);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.setSavedPasswordsListener_ = null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-section': PasswordsSectionElement;
  }
}

customElements.define(PasswordsSectionElement.is, PasswordsSectionElement);
