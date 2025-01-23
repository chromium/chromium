// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './password_change_page.html.js';

export class PasswordChangeSubpageElement extends PolymerElement {
  static get is() {
    return 'settings-password-change-subpage';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-password-change-subpage': PasswordChangeSubpageElement;
  }
}

customElements.define(
    PasswordChangeSubpageElement.is, PasswordChangeSubpageElement);
