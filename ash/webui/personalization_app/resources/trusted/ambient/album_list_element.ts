// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The element for displaying a list of albums.
 */

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './album_item_element.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AmbientModeAlbum, TopicSource} from '../personalization_app.mojom-webui.js';

export class AlbumList extends PolymerElement {
  static get is() {
    return 'album-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      topicSource: TopicSource,
      albums: {
        type: Array,
        value: null,
      },
    };
  }

  topicSource: TopicSource;
  albums: AmbientModeAlbum[]|null;
}

customElements.define(AlbumList.is, AlbumList);
