// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-completion-link-row' is the custom link row element for the
 * privacy guide completion card.
 */
import 'chrome://resources/cr_elements/cr_actionable_row_style.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTemplate} from './privacy_guide_completion_link_row.html.js';

export class PrivacyGuideCompletionLinkRowElement extends PolymerElement {
  static get is() {
    return 'privacy-guide-completion-link-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // Primary label of the link row.
      label: String,

      // Secondary label of the link row.
      subLabel: String,

      // The light mode source for the image of the link row.
      lightImgSrc: String,

      // The dark mode source for the image of the link row.
      darkImgSrc: String,
    };
  }
}

customElements.define(
    PrivacyGuideCompletionLinkRowElement.is,
    PrivacyGuideCompletionLinkRowElement);
