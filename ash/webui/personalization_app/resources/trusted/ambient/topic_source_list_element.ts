// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a list of topic (image) sources. It
 * behaviors similar to a radio button group, e.g. single selection.
 */

import 'chrome://personalization/trusted/ambient/topic_source_item_element.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
      hidden: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      topicSources: {
        type: Array,
        value: [TopicSource.kGooglePhotos, TopicSource.kArtGallery],
      },

      selectedTopicSource: TopicSource,

      hasGooglePhotosAlbums: Boolean,
    };
  }

  hidden: boolean;
  topicSources: Array<TopicSource>;
  selectedTopicSource: TopicSource;
  hasGooglePhotosAlbums: boolean;

  private onHiddenChanged_(hidden: boolean) {
    if (hidden) {
      return;
    }

    // When iron-list items change while their parent element is hidden, the
    // iron-list will render incorrectly. Force relayout by invalidating the
    // iron-list when this element becomes visible.
    afterNextRender(this, () => {
      this.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
    });
  }

  private isSelected_(
      topicSource: TopicSource, selectedTopicSource: TopicSource) {
    return selectedTopicSource === topicSource;
  }
}

customElements.define(TopicSourceList.is, TopicSourceList);
