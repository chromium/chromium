// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a list of topic (image) sources. It
 * behaviors similar to a radio button group, e.g. single selection.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './topic_source_item_element.js';

import {AmbientTheme, TopicSource} from '../../personalization_app.mojom-webui.js';
import {isTimeOfDayScreenSaverEnabled} from '../load_time_booleans.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './topic_source_list_element.html.js';
import {isValidTopicSourceAndTheme} from './utils.js';

export class TopicSourceListElement extends WithPersonalizationStore {
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
        value() {
          const topicSources =
              [TopicSource.kGooglePhotos, TopicSource.kArtGallery];
          // Pushes the video image source to the front to highlight exclusive
          // content.
          if (isTimeOfDayScreenSaverEnabled()) {
            topicSources.unshift(TopicSource.kVideo);
          }
          return topicSources;
        },
      },

      selectedAmbientTheme: AmbientTheme,

      selectedTopicSource: TopicSource,

      hasGooglePhotosAlbums: Boolean,
    };
  }

  topicSources: TopicSource[];
  selectedAmbientTheme: AmbientTheme;
  selectedTopicSource: TopicSource;
  hasGooglePhotosAlbums: boolean;

  override focus() {
    const elem = this.shadowRoot!.querySelector<HTMLElement>(
        'topic-source-item[checked]');
    if (elem) {
      elem.focus();
    }
  }

  private isTopicSourceDisabled_(
      topicSource: TopicSource, selectedAmbientTheme: AmbientTheme): boolean {
    return !isValidTopicSourceAndTheme(topicSource, selectedAmbientTheme);
  }

  private isSelected_(
      topicSource: TopicSource, selectedTopicSource: TopicSource) {
    return selectedTopicSource === topicSource;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-source-list': TopicSourceListElement;
  }
}

customElements.define(TopicSourceListElement.is, TopicSourceListElement);
