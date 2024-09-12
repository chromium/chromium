// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ai_tab_organization_subpage.html.js';

export class SettingsAiTabOrganizationSubpageElement extends PolymerElement {
  static get is() {
    return 'settings-ai-tab-organization-subpage';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-tab-organization-subpage':
        SettingsAiTabOrganizationSubpageElement;
  }
}

customElements.define(
    SettingsAiTabOrganizationSubpageElement.is,
    SettingsAiTabOrganizationSubpageElement);
