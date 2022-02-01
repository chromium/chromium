// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a topic (image) source.
 */

import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_style_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class TopicSourceItemElement extends PolymerElement {
  static get is() {
    return 'topic-source-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Whether this item is selected. This property is related to
       * cr_radio_button_style and used to style the disc appearance.
       */
      checked: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },
    };
  }
}

customElements.define(TopicSourceItemElement.is, TopicSourceItemElement);
