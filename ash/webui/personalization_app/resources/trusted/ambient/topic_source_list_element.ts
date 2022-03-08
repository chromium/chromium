// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a list of topic (image) sources. It
 * behaviors similar to a radio button group, e.g. single selection.
 */

import 'chrome://personalization/trusted/ambient/topic_source_item_element.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TopicSource} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

export class TopicSourceList extends WithPersonalizationStore {
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
        value: [TopicSource.kGooglePhotos, TopicSource.kArtGallery],
      },

      selectedTopicSource: TopicSource,

      hasGooglePhotosAlbums: Boolean,

      disabled: Boolean,
    };
  }

  topicSources: Array<TopicSource>;
  selectedTopicSource: TopicSource;
  hasGooglePhotosAlbums: boolean;
  disabled: boolean;

  private isSelected_(
      topicSource: TopicSource, selectedTopicSource: TopicSource) {
    return selectedTopicSource === topicSource;
  }

  private computeTabIndex_(tabIndex: number, disabled: boolean) {
    if (disabled) {
      return -1;
    }
    return tabIndex;
  }
}

customElements.define(TopicSourceList.is, TopicSourceList);
