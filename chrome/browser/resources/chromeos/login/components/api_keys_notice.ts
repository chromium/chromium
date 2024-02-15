// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './api_keys_notice.html.js';

/**
 * Simple container with a notice inside.
 * Shown when API keys are missing.
 */
export class ApiKeysNoticeElement extends PolymerElement {
  static get is() {
    return 'api-keys-notice-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      noticeContent: {
        value: '',
        type: String,
      },
    };
  }

  private noticeContent: string;

  constructor() {
    super();
    this.updateLocaleAndMaybeShowNotice();
  }

  updateLocaleAndMaybeShowNotice(): void {
    const missingApiId = 'missingAPIKeysNotice';
    if (!loadTimeData.valueExists(missingApiId)) {
      return;
    }

    this.noticeContent = loadTimeData.getValue(missingApiId);
    this.hidden = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ApiKeysNoticeElement.is]: ApiKeysNoticeElement;
  }
}

customElements.define(ApiKeysNoticeElement.is, ApiKeysNoticeElement);
