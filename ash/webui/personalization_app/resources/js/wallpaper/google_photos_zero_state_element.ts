// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element that displays the Google Photos zero state.
 */

import '../../css/wallpaper.css.js';
import '../../css/common.css.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './google_photos_zero_state_element.html.js';

export class GooglePhotosZeroState extends WithPersonalizationStore {
  static get is() {
    return 'google-photos-zero-state';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isDarkModeActive_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** Whether the page is being rendered in dark mode. */
  private isDarkModeActive_: boolean;

  /** Returns the message to be displayed. */
  private getMessage_(): TrustedHTML {
    return this.i18nAdvanced('googlePhotosZeroStateMessage', {
      substitutions: [
        '<a target="_blank" href="https://photos.google.com">photos.google.com</a>',
      ],
    });
  }

  private getImageSource_(): string {
    return this.isDarkModeActive_ ?
        'chrome://personalization/images/no_google_photos_images_dark.svg' :
        'chrome://personalization/images/no_google_photos_images.svg';
  }
}

customElements.define(GooglePhotosZeroState.is, GooglePhotosZeroState);
