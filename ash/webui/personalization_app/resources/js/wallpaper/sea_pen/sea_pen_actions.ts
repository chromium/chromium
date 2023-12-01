// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/store.js';

import {SeaPenQuery, SeaPenThumbnail} from '../../../sea_pen.mojom-webui.js';
import {SeaPenWallpaper} from '../constants.js';

/**
 * @fileoverview defines the actions to change SeaPen state.
 */

export enum SeaPenActionName {
  BEGIN_SEARCH_SEA_PEN_THUMBNAILS = 'begin_search_sea_pen_thumbnails',
  SET_SEA_PEN_THUMBNAILS = 'set_sea_pen_thumbnails',
  SET_RECENT_SEA_PEN_IMAGES = 'set_recent_sea_pen_images',
}

export type SeaPenActions = BeginSearchSeaPenThumbnailsAction|
    SetSeaPenThumbnailsAction|SetRecentSeaPenImagesAction;

export interface BeginSearchSeaPenThumbnailsAction extends Action {
  name: SeaPenActionName.BEGIN_SEARCH_SEA_PEN_THUMBNAILS;
  query: SeaPenQuery;
}

export function beginSearchSeaPenThumbnailsAction(query: SeaPenQuery):
    BeginSearchSeaPenThumbnailsAction {
  return {
    query: query,
    name: SeaPenActionName.BEGIN_SEARCH_SEA_PEN_THUMBNAILS,
  };
}

export interface SetSeaPenThumbnailsAction extends Action {
  name: SeaPenActionName.SET_SEA_PEN_THUMBNAILS;
  query: SeaPenQuery;
  images: SeaPenThumbnail[]|null;
}

/**
 * Sets the generated thumbnails for the given prompt text.
 */
export function setSeaPenThumbnailsAction(
    query: SeaPenQuery,
    images: SeaPenThumbnail[]|null): SetSeaPenThumbnailsAction {
  return {name: SeaPenActionName.SET_SEA_PEN_THUMBNAILS, query, images};
}

export interface SetRecentSeaPenImagesAction extends Action {
  name: SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES;
  recentWallpapers: SeaPenWallpaper[]|null;
}

/**
 * Sets the recent SeaPen wallpapers.
 */
export function setRecentSeaPenImagesAction(recentWallpapers: SeaPenWallpaper[]|
                                            null): SetRecentSeaPenImagesAction {
  return {
    name: SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES,
    recentWallpapers,
  };
}
