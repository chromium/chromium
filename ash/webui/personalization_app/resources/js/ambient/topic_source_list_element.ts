// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a list of topic (image) sources. It
 * behaviors similar to a radio button group, e.g. single selection.
 */

import '../../css/common.css.js';
import './topic_source_item_element.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {TopicSource} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './topic_source_list_element.html.js';

export class TopicSourceList extends WithPersonalizationStore {
  static get is() {
    return 'topic-source-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      topicSources: {
        type: Array,
        value: [TopicSource.kGooglePhotos, TopicSource.kArtGallery],
      },

      selectedTopicSource: TopicSource,

      hasGooglePhotosAlbums: Boolean,
    };
  }

  topicSources: TopicSource[];
  selectedTopicSource: TopicSource;
  hasGooglePhotosAlbums: boolean;

  private isSelected_(
      topicSource: TopicSource, selectedTopicSource: TopicSource) {
    return selectedTopicSource === topicSource;
  }
}

customElements.define(TopicSourceList.is, TopicSourceList);
