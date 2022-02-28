// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ambient albums subpage is to select personal albums in
 * Google Photos or categories in Art gallery.
 */

import 'chrome://resources/cr_components/chromeos/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './album_list_element.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isNonEmptyArray} from '../../common/utils.js';
import {AmbientModeAlbum, TopicSource} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

export class AlbumsSubpage extends WithPersonalizationStore {
  static get is() {
    return 'albums-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      topicSource: TopicSource,
      albums: {
        type: Array,
        // Set to null to differentiate from an empty album.
        value: null,
      },
    };
  }

  topicSource: TopicSource;
  albums: AmbientModeAlbum[]|null = null;

  private getTitleInnerHtml_(): string {
    if (this.topicSource === TopicSource.kGooglePhotos) {
      return this.i18nAdvanced('ambientModeAlbumsSubpageGooglePhotosTitle');
    } else {
      return this.i18n('ambientModeTopicSourceArtGalleryDescription');
    }
  }

  private hasAlbums_(): boolean {
    return isNonEmptyArray(this.albums);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'albums-subpage': AlbumsSubpage;
  }
}

customElements.define(AlbumsSubpage.is, AlbumsSubpage);
