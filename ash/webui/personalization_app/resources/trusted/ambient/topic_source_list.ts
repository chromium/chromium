// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a list of topic (image) sources. It
 * behaviors similar to a radio button group, e.g. single selection.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class TopicSourceListElement extends PolymerElement {
  static get is() {
    return 'topic-source-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      topicSources: {
        type: Array,
        value: ['art', 'photos'],
      },
    };
  }
}

customElements.define(TopicSourceListElement.is, TopicSourceListElement);
