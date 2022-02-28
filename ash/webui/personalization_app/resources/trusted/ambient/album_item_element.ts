// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying an album.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isSelectionEvent} from '../../common/utils.js';
import {AmbientModeAlbum, TopicSource} from '../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isRecentHighlightsAlbum} from '../utils.js';

import {setAlbumSelected} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';

export class AlbumItem extends WithPersonalizationStore {
  static get is() {
    return 'album-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      album: {
        type: AmbientModeAlbum,
        value: null,
      },
      topicSource: TopicSource,
      checked: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
        notify: true,
      },
      ariaLabel: {
        type: String,
        computed: 'computeAriaLabel_(album, checked, topicSource)',
        reflectToAttribute: true,
      },
      itemDescription_: {
        type: String,
        computed: 'computeItemDescription_(album.*, topicSource)',
      },
    };
  }

  album: AmbientModeAlbum|null;
  topicSource: TopicSource;
  checked: boolean;
  ariaLabel: string;
  private itemDescription_: string;

  ready() {
    super.ready();

    this.addEventListener('keydown', this.onItemSelected_.bind(this));
  }

  private computeClass_(): string {
    return this.topicSource === TopicSource.kGooglePhotos ? 'personal-album' :
                                                            'art-album';
  }

  private computeItemDescription_(): string {
    if (!this.album) {
      return '';
    }

    if (this.topicSource === TopicSource.kGooglePhotos) {
      if (isRecentHighlightsAlbum(this.album)) {
        return this.i18n('ambientModeAlbumsSubpageRecentHighlightsDesc');
      }

      if (this.album.numberOfPhotos <= 1) {
        return this.i18n(
            'ambientModeAlbumsSubpagePhotosNumSingularDesc',
            this.album.numberOfPhotos);
      }

      return this.i18n(
          'ambientModeAlbumsSubpagePhotosNumPluralDesc',
          this.album.numberOfPhotos);
    }

    if (this.topicSource === TopicSource.kArtGallery) {
      return this.album.description;
    }

    return '';
  }

  private computeAriaLabel_(): string {
    if (!this.album) {
      return '';
    }

    if (this.album.checked) {
      return this.i18n(
          'ambientModeAlbumsSubpageAlbumSelected', this.album.title,
          this.itemDescription_);
    }

    return this.i18n(
        'ambientModeAlbumsSubpageAlbumUnselected', this.album.title,
        this.itemDescription_);
  }

  private onImageClick_(event: Event) {
    this.onItemSelected_(event);
  }

  private onItemSelected_(event: Event) {
    if (!this.album) {
      return;
    }

    if (!isSelectionEvent(event)) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    this.checked = !this.checked;
    setAlbumSelected(this.album, getAmbientProvider());
  }
}

customElements.define(AlbumItem.is, AlbumItem);
