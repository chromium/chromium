// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/store.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {SeaPenQuery, SeaPenThumbnail} from '../../../sea_pen.mojom-webui.js';

import {RecentSeaPenData} from './constants.js';

/**
 * @fileoverview defines the actions to change SeaPen state.
 */

export enum SeaPenActionName {
  BEGIN_SEARCH_SEA_PEN_THUMBNAILS = 'begin_search_sea_pen_thumbnails',
  BEGIN_LOAD_RECENT_SEA_PEN_IMAGES = 'begin_load_recent_sea_pen_images',
  BEGIN_LOAD_RECENT_SEA_PEN_IMAGE_DATA = 'begin_load_recent_sea_pen_image_data',
  SET_SEA_PEN_THUMBNAILS = 'set_sea_pen_thumbnails',
  SET_RECENT_SEA_PEN_IMAGES = 'set_recent_sea_pen_images',
  SET_RECENT_SEA_PEN_IMAGE_DATA = 'set_recent_sea_pen_image_data',
}

export type SeaPenActions =
    BeginSearchSeaPenThumbnailsAction|BeginLoadRecentSeaPenImagesAction|
    BeginLoadRecentSeaPenImageDataAction|SetSeaPenThumbnailsAction|
    SetRecentSeaPenImagesAction|SetRecentSeaPenImageDataAction;

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

export interface BeginLoadRecentSeaPenImagesAction extends Action {
  name: SeaPenActionName.BEGIN_LOAD_RECENT_SEA_PEN_IMAGES;
}

/**
 * Begins load recent sea pen images.
 */
export function beginLoadRecentSeaPenImagesAction():
    BeginLoadRecentSeaPenImagesAction {
  return {
    name: SeaPenActionName.BEGIN_LOAD_RECENT_SEA_PEN_IMAGES,
  };
}

export interface SetRecentSeaPenImagesAction extends Action {
  name: SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES;
  recentImages: FilePath[]|null;
}

/**
 * Sets the recent sea pen images.
 */
export function setRecentSeaPenImagesAction(recentImages: FilePath[]|
                                            null): SetRecentSeaPenImagesAction {
  return {
    name: SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES,
    recentImages,
  };
}

export interface BeginLoadRecentSeaPenImageDataAction extends Action {
  name: SeaPenActionName.BEGIN_LOAD_RECENT_SEA_PEN_IMAGE_DATA;
  id: string;
}

/**
 * Begins load the recent sea pen image data.
 */
export function beginLoadRecentSeaPenImageDataAction(image: FilePath):
    BeginLoadRecentSeaPenImageDataAction {
  return {
    name: SeaPenActionName.BEGIN_LOAD_RECENT_SEA_PEN_IMAGE_DATA,
    id: image.path,
  };
}

export interface SetRecentSeaPenImageDataAction extends Action {
  name: SeaPenActionName.SET_RECENT_SEA_PEN_IMAGE_DATA;
  id: string;
  data: RecentSeaPenData;
}

/**
 * Sets the recent sea pen image data.
 */
export function setRecentSeaPenImageDataAction(
    filePath: FilePath,
    data: RecentSeaPenData): SetRecentSeaPenImageDataAction {
  return {
    name: SeaPenActionName.SET_RECENT_SEA_PEN_IMAGE_DATA,
    id: filePath.path,
    data,
  };
}
