// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that displays the freeform subpage.
 */

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FreeformTab} from './constants.js';
import {getTemplate} from './sea_pen_freeform_element.html.js';

const SeaPenFreeformElementBase = I18nMixin(PolymerElement);

export class SeaPenFreeformElement extends SeaPenFreeformElementBase {
  static get is() {
    return 'sea-pen-freeform';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      freeformTab_: {
        type: String,
        value: FreeformTab.SAMPLE_PROMPTS,
      },
    };
  }

  private freeformTab_: FreeformTab;

  private isSamplePromptsTabSelected_(tab: FreeformTab): boolean {
    return tab === FreeformTab.SAMPLE_PROMPTS;
  }

  private onRecentFreeformImageDelete_() {
    // TODO(b/347328001): add the function implementation.
  }
}

customElements.define(SeaPenFreeformElement.is, SeaPenFreeformElement);
