// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a topic (image) source.
 */

import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {TopicSource} from '../../personalization_app.mojom-webui.js';
import {PersonalizationRouterElement} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {isSelectionEvent} from '../utils.js';

import {setTopicSource} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {getTemplate} from './topic_source_item_element.html.js';
import {getTopicSourceName} from './utils.js';

export class TopicSourceItemElement extends WithPersonalizationStore {
  static get is() {
    return 'topic-source-item';
  }

  static get template() {
    return getTemplate();
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
        observer: 'onCheckedChanged_',
      },

      disabled: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      topicSource: TopicSource,

      hasGooglePhotosAlbums: {
        type: Boolean,
        value: null,
      },

      ariaLabel: {
        type: String,
        computed: 'computeAriaLabel_(topicSource, checked)',
        reflectToAttribute: true,
      },
    };
  }

  checked: boolean;
  disabled: boolean;
  topicSource: TopicSource;
  hasGooglePhotosAlbums: boolean|null;
  override ariaLabel: string;

  override ready() {
    super.ready();

    this.addEventListener('click', this.onItemSelected_.bind(this));
    this.addEventListener('keydown', this.onItemSelected_.bind(this));
  }

  private onCheckedChanged_(value: boolean) {
    this.setAttribute('aria-checked', value.toString());
  }

  private onItemSelected_(event: Event) {
    if (!isSelectionEvent(event)) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    setTopicSource(this.topicSource, getAmbientProvider(), this.getStore());
    PersonalizationRouterElement.instance().selectAmbientAlbums(
        this.topicSource);
  }

  private getItemName_(): string {
    return getTopicSourceName(this.topicSource);
  }

  private getItemDescription_(): string {
    if (this.topicSource === TopicSource.kGooglePhotos) {
      if (!this.hasGooglePhotosAlbums) {
        return '';
      }
      if (this.hasGooglePhotosAlbums) {
        return this.i18n('ambientModeTopicSourceGooglePhotosDescription');
      } else {
        return this.i18n(
            'ambientModeTopicSourceGooglePhotosDescriptionNoAlbum');
      }
    } else if (this.topicSource === TopicSource.kArtGallery) {
      return this.i18n('ambientModeTopicSourceArtGalleryDescription');
    } else if (this.topicSource === TopicSource.kVideo) {
      return this.i18n('ambientModeTopicSourceVideoDescription');
    } else {
      return '';
    }
  }

  private computeAriaLabel_(): string {
    // topicSource may be undefined when aria label is computed the first time.
    if (this.topicSource === undefined) {
      return '';
    }
    if (this.checked) {
      return this.i18n(
          'ambientModeTopicSourceSelectedRow', this.getItemName_(),
          this.getItemDescription_());
    }
    return this.i18n(
        'ambientModeTopicSourceUnselectedRow', this.getItemName_(),
        this.getItemDescription_());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-source-item': TopicSourceItemElement;
  }
}

customElements.define(TopicSourceItemElement.is, TopicSourceItemElement);
