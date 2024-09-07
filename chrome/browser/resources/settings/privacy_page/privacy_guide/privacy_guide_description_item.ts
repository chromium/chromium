// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-description-item' is a bullet-point-style item in
 * the description of a privacy setting in the privacy guide.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../../settings_shared.css.js';

import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_guide_description_item.html.js';

export class PrivacyGuideDescriptionItemElement extends PolymerElement {
  static get is() {
    return 'privacy-guide-description-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      icon: {
        type: String,
        value: '',
      },

      label: {
        type: String,
        value: '',
      },

      labelHtml: {
        type: String,
        value: '',
      },
    };
  }

  label: string;

  private sanitizeInnerHtml_(rawString: string): TrustedHTML {
    return sanitizeInnerHtml(rawString);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-guide-description-item': PrivacyGuideDescriptionItemElement;
  }
}

customElements.define(
    PrivacyGuideDescriptionItemElement.is, PrivacyGuideDescriptionItemElement);
