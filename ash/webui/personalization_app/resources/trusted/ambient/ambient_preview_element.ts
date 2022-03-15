// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that previews the current selected
 * screensaver.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import '../../common/styles.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AmbientModeAlbum, TopicSource} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getPhotoCount, getTopicSourceName} from '../utils.js';

import {AmbientObserver} from './ambient_observer.js';

export class AmbientPreview extends WithPersonalizationStore {
  static get is() {
    return 'ambient-preview';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      albums_: {
        type: Array,
        value: null,
      },
      topicSource_: {
        type: Object,
        value: null,
      },
      previewAlbum_: {
        type: AmbientModeAlbum,
        computed: 'computePreviewAlbum_(albums_, topicSource_)',
      }
    };
  }

  private albums_: AmbientModeAlbum[]|null;
  private topicSource_: TopicSource|null;
  private previewAlbum_: AmbientModeAlbum|null;

  override ready() {
    super.ready();
    AmbientObserver.initAmbientObserverIfNeeded();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch('albums_', state => state.ambient.albums);
    this.watch('topicSource_', state => state.ambient.topicSource);
    this.updateFromStore();
  }

  private computePreviewAlbum_(): AmbientModeAlbum|null {
    // TODO(b/222712399): handle preview collage and text for multiple albums.
    // Currently, if there are multiple albums selected, return the first
    // selected album.
    if (!this.albums_) {
      return null;
    }
    return this.albums_.find(
               album => album.topicSource === this.topicSource_ &&
                   album.checked && album.url) ??
        null;
  }

  private getPreviewImage_(): string {
    return this.previewAlbum_ && this.previewAlbum_.url ?
        this.previewAlbum_.url.url :
        '';
  }

  private getAlbumTitle_(): string {
    return this.previewAlbum_ && this.previewAlbum_.title ?
        this.previewAlbum_.title :
        '';
  }

  private getAlbumDescription_(): string {
    if (!this.previewAlbum_ || this.topicSource_ === null) {
      return '';
    }
    const topicSourceDesc = getTopicSourceName(this.topicSource_);
    // TODO(b/223834394): replace dot separator symbol • with an icon/image.
    // As we don't know the number of photos in Art Gallery albums,
    // this info won't be shown in this case.
    return this.topicSource_ === TopicSource.kArtGallery ?
        topicSourceDesc :
        `${topicSourceDesc} • ${getPhotoCount(this.previewAlbum_)}`;
  }
}

customElements.define(AmbientPreview.is, AmbientPreview);
