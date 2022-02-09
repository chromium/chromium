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

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TopicSource} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {setTopicSource} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';

export class TopicSourceItemElement extends WithPersonalizationStore {
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
        value: false,
        reflectToAttribute: true,
      },

      topicSource: TopicSource,
    };
  }

  checked: boolean;
  topicSource: TopicSource;

  ready() {
    super.ready();

    this.addEventListener('click', this.onItemSelected_);
  }

  private onItemSelected_(event: Event) {
    event.stopPropagation();
    setTopicSource(this.topicSource, getAmbientProvider(), this.getStore());
  }
}

customElements.define(TopicSourceItemElement.is, TopicSourceItemElement);
