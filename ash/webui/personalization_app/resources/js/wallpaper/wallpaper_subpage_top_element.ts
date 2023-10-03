// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that shows the top element of the wallpaper
 * subpage.
 */

import {isSeaPenEnabled, isSeaPenTextInputEnabled} from '../load_time_booleans.js';
import {Paths} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './wallpaper_subpage_top_element.html.js';

export class WallpaperSubpageTopElement extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-subpage-top';
  }
  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current collection id to display.
       */
      collectionId: String,
      /**
       * The current Google Photos Album id to display.
       */
      googlePhotosAlbumId: String,
      /**
       * Whether the Google Photos album is shared.
       */
      isGooglePhotosAlbumShared: {
        type: Boolean,
        value: false,
      },
      /**
       * The current path of the page.
       */
      path: String,
      /**
       * The sea pen template id.
       */
      templateId: {
        type: String,
      },
    };
  }

  collectionId: string|undefined;
  googlePhotosAlbumId: string|undefined;
  isGooglePhotosAlbumShared: boolean;
  path: string;
  templateId: string|null;

  private shouldShowInputQuery_(path: string, templateId: string|null):
      boolean {
    return isSeaPenTextInputEnabled() && path === Paths.SEA_PEN_COLLECTION &&
        !templateId;
  }

  private shouldShowTemplateQuery_(path: string, templateId: string|null):
      boolean {
    return isSeaPenEnabled() && path === Paths.SEA_PEN_COLLECTION &&
        !!templateId;
  }

  private shouldShowWallpaperSelectedElement_(
      path: string, templateId: string|null): boolean {
    return !this.shouldShowInputQuery_(path, templateId) &&
        !this.shouldShowTemplateQuery_(path, templateId);
  }
}
customElements.define(
    WallpaperSubpageTopElement.is, WallpaperSubpageTopElement);
