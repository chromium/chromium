// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {Action} from 'chrome://resources/js/store.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {RecentSeaPenData} from './constants.js';
import {MantaStatusCode, SeaPenQuery, SeaPenThumbnail} from './sea_pen.mojom-webui.js';

/**
 * @fileoverview defines the actions to change SeaPen state.
 */

export enum SeaPenActionName {
  BEGIN_SEARCH_SEA_PEN_THUMBNAILS = 'begin_search_sea_pen_thumbnails',
  BEGIN_LOAD_RECENT_SEA_PEN_IMAGES = 'begin_load_recent_sea_pen_images',
  BEGIN_LOAD_RECENT_SEA_PEN_IMAGE_DATA = 'begin_load_recent_sea_pen_image_data',
  BEGIN_LOAD_SELECTED_RECENT_SEA_PEN_IMAGE =
      'begin_load_selected_recent_sea_pen_image',
  SET_THUMBNAIL_RESPONSE_STATUS_CODE = 'set_thumbnail_response_status_code',
  BEGIN_SELECT_SEA_PEN_THUMBNAIL = 'begin_select_sea_pen_thumbnail',
  CLEAR_SEA_PEN_THUMBNAILS = 'clear_sea_pen_thumbnails',
  END_SELECT_SEA_PEN_THUMBNAIL = 'end_select_sea_pen_thumbnail',
  BEGIN_SELECT_RECENT_SEA_PEN_IMAGE = 'begin_select_recent_sea_pen_image',
  END_SELECT_RECENT_SEA_PEN_IMAGE = 'end_select_recent_sea_pen_image',
  SET_SEA_PEN_THUMBNAILS = 'set_sea_pen_thumbnails',
  SET_RECENT_SEA_PEN_IMAGES = 'set_recent_sea_pen_images',
  SET_RECENT_SEA_PEN_IMAGE_DATA = 'set_recent_sea_pen_image_data',
  SET_SELECTED_RECENT_SEA_PEN_IMAGE = 'set_selected_recent_sea_pen_image',
  SET_SHOULD_SHOW_SEA_PEN_TERMS_OF_SERVICE_DIALOG =
      'set_should_show_sea_pen_terms_of_service_dialog',
}

export type SeaPenActions = BeginSearchSeaPenThumbnailsAction|
    BeginLoadRecentSeaPenImagesAction|BeginLoadRecentSeaPenImageDataAction|
    BeginLoadSelectedRecentSeaPenImageAction|BeginSelectRecentSeaPenImageAction|
    ClearSeaPenThumbnailsAction|EndSelectRecentSeaPenImageAction|
    SetThumbnailResponseStatusCodeAction|SetSeaPenThumbnailsAction|
    SetRecentSeaPenImagesAction|SetRecentSeaPenImageDataAction|
    SetSelectedRecentSeaPenImageAction|BeginSelectSeaPenThumbnailAction|
    EndSelectSeaPenThumbnailAction|
    SetShouldShowSeaPenTermsOfServiceDialogAction;

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

export interface BeginSelectRecentSeaPenImageAction extends Action {
  name: SeaPenActionName.BEGIN_SELECT_RECENT_SEA_PEN_IMAGE;
  image: FilePath;
}

/**
 * Begins selecting a recent Sea Pen image.
 */
export function beginSelectRecentSeaPenImageAction(image: FilePath):
    BeginSelectRecentSeaPenImageAction {
  return {
    name: SeaPenActionName.BEGIN_SELECT_RECENT_SEA_PEN_IMAGE,
    image: image,
  };
}

export interface EndSelectRecentSeaPenImageAction extends Action {
  name: SeaPenActionName.END_SELECT_RECENT_SEA_PEN_IMAGE;
  image: FilePath;
  success: boolean;
}

/**
 * Ends selecting a recent Sea Pen image.
 */
export function endSelectRecentSeaPenImageAction(
    image: FilePath, success: boolean): EndSelectRecentSeaPenImageAction {
  return {
    name: SeaPenActionName.END_SELECT_RECENT_SEA_PEN_IMAGE,
    image,
    success,
  };
}

export interface BeginLoadSelectedRecentSeaPenImageAction extends Action {
  name: SeaPenActionName.BEGIN_LOAD_SELECTED_RECENT_SEA_PEN_IMAGE;
}

/**
 * Begins loading the selected recent Sea Pen image.
 */
export function beginLoadSelectedRecentSeaPenImageAction():
    BeginLoadSelectedRecentSeaPenImageAction {
  return {name: SeaPenActionName.BEGIN_LOAD_SELECTED_RECENT_SEA_PEN_IMAGE};
}

export interface SetSelectedRecentSeaPenImageAction extends Action {
  name: SeaPenActionName.SET_SELECTED_RECENT_SEA_PEN_IMAGE;
  key: string|null;
}

/**
 * Sets the selected recent Sea Pen image.
 */
export function setSelectedRecentSeaPenImageAction(key: string|null):
    SetSelectedRecentSeaPenImageAction {
  return {
    name: SeaPenActionName.SET_SELECTED_RECENT_SEA_PEN_IMAGE,
    key: key,
  };
}

/** Sets the Sea Pen thumbnail response status code. */
export interface SetThumbnailResponseStatusCodeAction extends Action {
  name: SeaPenActionName.SET_THUMBNAIL_RESPONSE_STATUS_CODE;
  thumbnailResponseStatusCode: MantaStatusCode|null;
}

export function setThumbnailResponseStatusCodeAction(
    thumbnailResponseStatusCode: MantaStatusCode|
    null): SetThumbnailResponseStatusCodeAction {
  return {
    name: SeaPenActionName.SET_THUMBNAIL_RESPONSE_STATUS_CODE,
    thumbnailResponseStatusCode,
  };
}

export interface BeginSelectSeaPenThumbnailAction extends Action {
  name: SeaPenActionName.BEGIN_SELECT_SEA_PEN_THUMBNAIL;
  thumbnail: SeaPenThumbnail;
}

export function beginSelectSeaPenThumbnailAction(thumbnail: SeaPenThumbnail):
    BeginSelectSeaPenThumbnailAction {
  return {
    name: SeaPenActionName.BEGIN_SELECT_SEA_PEN_THUMBNAIL,
    thumbnail,
  };
}

export interface EndSelectSeaPenThumbnailAction extends Action {
  name: SeaPenActionName.END_SELECT_SEA_PEN_THUMBNAIL;
  thumbnail: SeaPenThumbnail;
  success: boolean;
}

export function endSelectSeaPenThumbnailAction(
    thumbnail: SeaPenThumbnail,
    success: boolean): EndSelectSeaPenThumbnailAction {
  return {
    name: SeaPenActionName.END_SELECT_SEA_PEN_THUMBNAIL,
    thumbnail,
    success,
  };
}

export interface ClearSeaPenThumbnailsAction extends Action {
  name: SeaPenActionName.CLEAR_SEA_PEN_THUMBNAILS;
}

export function clearSeaPenThumbnailsAction(): ClearSeaPenThumbnailsAction {
  return {name: SeaPenActionName.CLEAR_SEA_PEN_THUMBNAILS};
}

export interface SetShouldShowSeaPenTermsOfServiceDialogAction extends Action {
  name: SeaPenActionName.SET_SHOULD_SHOW_SEA_PEN_TERMS_OF_SERVICE_DIALOG;
  shouldShowDialog: boolean;
}

/**
 * Sets the boolean that determines whether to show the Sea Pen terms of service
 * dialog.
 */
export function setShouldShowSeaPenTermsOfServiceDialogAction(
    shouldShowDialog: boolean): SetShouldShowSeaPenTermsOfServiceDialogAction {
  assert(typeof shouldShowDialog === 'boolean');
  return {
    name: SeaPenActionName.SET_SHOULD_SHOW_SEA_PEN_TERMS_OF_SERVICE_DIALOG,
    shouldShowDialog,
  };
}
