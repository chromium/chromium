// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays the Google Photos zero state.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GooglePhotosTab} from './google_photos_collection_element.js';
import {getTemplate} from './google_photos_zero_state_element.html.js';

const Base = I18nMixin(PolymerElement);

export class GooglePhotosZeroStateElement extends Base {
  static get is() {
    return 'google-photos-zero-state';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      tab: String,
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  tab: GooglePhotosTab;
  /** Whether the page is being rendered in dark mode. */
  private isDarkModeActive_: boolean;

  private getMessageLabel_(tab: GooglePhotosTab): string {
    switch (tab) {
      case GooglePhotosTab.ALBUMS:
      case GooglePhotosTab.PHOTOS:
        return 'googlePhotosZeroStateMessage';
      case GooglePhotosTab.PHOTOS_BY_ALBUM_ID:
        return 'googlePhotosAlbumZeroStateMessage';
      default:
        assertNotReached(
            `valid 'GooglePhotosTab' expected but received ${tab}`);
    }
  }

  /** Returns the message to be displayed. */
  private getMessage_(tab: GooglePhotosTab): TrustedHTML {
    const label = this.getMessageLabel_(tab);
    return this.i18nAdvanced(label, {
      substitutions: [
        `<a target="_blank" href="${
            this.i18n('googlePhotosURL')}">photos.google.com</a>`,
      ],
    });
  }

  private getImageSource_(): string {
    return this.isDarkModeActive_ ?
        'chrome://personalization/images/no_google_photos_images_dark.svg' :
        'chrome://personalization/images/no_google_photos_images.svg';
  }
}

customElements.define(
    GooglePhotosZeroStateElement.is, GooglePhotosZeroStateElement);
