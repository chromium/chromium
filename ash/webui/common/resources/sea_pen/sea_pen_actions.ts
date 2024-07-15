// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FullscreenPreviewState} from 'chrome://resources/ash/common/personalization/wallpaper_state.js';
import {assert} from 'chrome://resources/js/assert.js';
import {Action} from 'chrome://resources/js/store.js';

import {SeaPenImageId} from './constants.js';
import {MantaStatusCode, RecentSeaPenThumbnailData, SeaPenQuery, SeaPenThumbnail, TextQueryHistoryEntry} from './sea_pen.mojom-webui.js';

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
  CLEAR_CURRENT_SEA_PEN_QUERY = 'clear_current_sea_pen_query',
  CLEAR_SEA_PEN_THUMBNAILS = 'clear_sea_pen_thumbnails',
  CLEAR_SEA_PEN_THUMBNAILS_LOADING = 'clear_sea_pen_thumbnails_loading',
  END_SELECT_SEA_PEN_THUMBNAIL = 'end_select_sea_pen_thumbnail',
  BEGIN_SELECT_RECENT_SEA_PEN_IMAGE = 'begin_select_recent_sea_pen_image',
  END_SELECT_RECENT_SEA_PEN_IMAGE = 'end_select_recent_sea_pen_image',
  SET_CURRENT_SEA_PEN_QUERY = 'set_current_sea_pen_query',
  SET_SEA_PEN_THUMBNAILS = 'set_sea_pen_thumbnails',
  SET_RECENT_SEA_PEN_IMAGES = 'set_recent_sea_pen_images',
  SET_RECENT_SEA_PEN_IMAGE_DATA = 'set_recent_sea_pen_image_data',
  SET_SELECTED_RECENT_SEA_PEN_IMAGE = 'set_selected_recent_sea_pen_image',
  SET_SHOULD_SHOW_SEA_PEN_INTRODUCTION_DIALOG =
      'set_should_show_sea_pen_introduction_dialog',
  DISMISS_SEA_PEN_ERROR_ACTION = 'dismiss_sea_pen_error',
  SET_SEA_PEN_FULLSCREEN_STATE = 'set_sea_pen_fullscreen_state',
  SET_SEA_PEN_TEXT_QUERY_HISTORY = 'set_sea_pen_text_query_history',
}

export type SeaPenActions = BeginSearchSeaPenThumbnailsAction|
    BeginLoadRecentSeaPenImagesAction|BeginLoadRecentSeaPenImageDataAction|
    BeginLoadSelectedRecentSeaPenImageAction|BeginSelectRecentSeaPenImageAction|
    ClearCurrentSeaPenQueryAction|ClearSeaPenThumbnailsAction|
    ClearSeaPenThumbnailsLoadingAction|EndSelectRecentSeaPenImageAction|
    SetThumbnailResponseStatusCodeAction|SetCurrentSeaPenQueryAction|
    SetSeaPenThumbnailsAction|SetRecentSeaPenImagesAction|
    SetRecentSeaPenImageDataAction|SetSelectedRecentSeaPenImageAction|
    BeginSelectSeaPenThumbnailAction|EndSelectSeaPenThumbnailAction|
    SetShouldShowSeaPenIntroductionDialogAction|DismissSeaPenErrorAction|
    SetSeaPenFullscreenStateAction|SetSeaPenTextQueryHistory;

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

export interface SetCurrentSeaPenQueryAction extends Action {
  name: SeaPenActionName.SET_CURRENT_SEA_PEN_QUERY;
  query: SeaPenQuery;
}

/**
 * Sets the currently searched Sea Pen query.
 */
export function setCurrentSeaPenQueryAction(query: SeaPenQuery):
    SetCurrentSeaPenQueryAction {
  return {name: SeaPenActionName.SET_CURRENT_SEA_PEN_QUERY, query};
}

export interface SetSeaPenThumbnailsAction extends Action {
  name: SeaPenActionName.SET_SEA_PEN_THUMBNAILS;
  query: SeaPenQuery;
  thumbnails: SeaPenThumbnail[]|null;
}

/**
 * Sets the generated thumbnails for the given prompt text.
 */
export function setSeaPenThumbnailsAction(
    query: SeaPenQuery,
    thumbnails: SeaPenThumbnail[]|null): SetSeaPenThumbnailsAction {
  return {name: SeaPenActionName.SET_SEA_PEN_THUMBNAILS, query, thumbnails};
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
  recentImages: SeaPenImageId[]|null;
}

/**
 * Sets the recent sea pen images.
 */
export function setRecentSeaPenImagesAction(recentImages: SeaPenImageId[]|
                                            null): SetRecentSeaPenImagesAction {
  return {
    name: SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES,
    recentImages,
  };
}

export interface BeginLoadRecentSeaPenImageDataAction extends Action {
  name: SeaPenActionName.BEGIN_LOAD_RECENT_SEA_PEN_IMAGE_DATA;
  id: SeaPenImageId;
}

/**
 * Begins load the recent sea pen image data.
 */
export function beginLoadRecentSeaPenImageDataAction(id: SeaPenImageId):
    BeginLoadRecentSeaPenImageDataAction {
  return {
    name: SeaPenActionName.BEGIN_LOAD_RECENT_SEA_PEN_IMAGE_DATA,
    id,
  };
}

export interface SetRecentSeaPenImageDataAction extends Action {
  name: SeaPenActionName.SET_RECENT_SEA_PEN_IMAGE_DATA;
  id: SeaPenImageId;
  data: RecentSeaPenThumbnailData|null;
}

/**
 * Sets the recent sea pen image data.
 */
export function setRecentSeaPenImageDataAction(
    id: SeaPenImageId,
    data: RecentSeaPenThumbnailData|null): SetRecentSeaPenImageDataAction {
  return {
    name: SeaPenActionName.SET_RECENT_SEA_PEN_IMAGE_DATA,
    id,
    data,
  };
}

export interface BeginSelectRecentSeaPenImageAction extends Action {
  name: SeaPenActionName.BEGIN_SELECT_RECENT_SEA_PEN_IMAGE;
  id: SeaPenImageId;
}

/**
 * Begins selecting a recent Sea Pen image.
 */
export function beginSelectRecentSeaPenImageAction(id: SeaPenImageId):
    BeginSelectRecentSeaPenImageAction {
  return {
    name: SeaPenActionName.BEGIN_SELECT_RECENT_SEA_PEN_IMAGE,
    id,
  };
}

export interface EndSelectRecentSeaPenImageAction extends Action {
  name: SeaPenActionName.END_SELECT_RECENT_SEA_PEN_IMAGE;
  id: SeaPenImageId;
  success: boolean;
}

/**
 * Ends selecting a recent Sea Pen image.
 */
export function endSelectRecentSeaPenImageAction(
    id: SeaPenImageId, success: boolean): EndSelectRecentSeaPenImageAction {
  return {
    name: SeaPenActionName.END_SELECT_RECENT_SEA_PEN_IMAGE,
    id,
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
  key: SeaPenImageId|null;
}

/**
 * Sets the selected recent Sea Pen image.
 * Key is the id of the thumbnail that was used to generate this image.
 */
export function setSelectedRecentSeaPenImageAction(key: SeaPenImageId|null):
    SetSelectedRecentSeaPenImageAction {
  return {
    name: SeaPenActionName.SET_SELECTED_RECENT_SEA_PEN_IMAGE,
    key,
  };
}

export interface SetSeaPenTextQueryHistory extends Action {
  name: SeaPenActionName.SET_SEA_PEN_TEXT_QUERY_HISTORY;
  history: TextQueryHistoryEntry[]|null;
}

export function setSeaPenTextQueryHistory(history: TextQueryHistoryEntry[]|
                                          null): SetSeaPenTextQueryHistory {
  return {
    name: SeaPenActionName.SET_SEA_PEN_TEXT_QUERY_HISTORY,
    history,
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

export interface ClearCurrentSeaPenQueryAction extends Action {
  name: SeaPenActionName.CLEAR_CURRENT_SEA_PEN_QUERY;
}

export function clearCurrentSeaPenQueryAction(): ClearCurrentSeaPenQueryAction {
  return {name: SeaPenActionName.CLEAR_CURRENT_SEA_PEN_QUERY};
}

export interface ClearSeaPenThumbnailsAction extends Action {
  name: SeaPenActionName.CLEAR_SEA_PEN_THUMBNAILS;
}

export function clearSeaPenThumbnailsAction(): ClearSeaPenThumbnailsAction {
  return {name: SeaPenActionName.CLEAR_SEA_PEN_THUMBNAILS};
}

export interface ClearSeaPenThumbnailsLoadingAction extends Action {
  name: SeaPenActionName.CLEAR_SEA_PEN_THUMBNAILS_LOADING;
}

export function clearSeaPenThumbnailsLoadingAction():
    ClearSeaPenThumbnailsLoadingAction {
  return {name: SeaPenActionName.CLEAR_SEA_PEN_THUMBNAILS_LOADING};
}

export interface SetShouldShowSeaPenIntroductionDialogAction extends Action {
  name: SeaPenActionName.SET_SHOULD_SHOW_SEA_PEN_INTRODUCTION_DIALOG;
  shouldShowDialog: boolean;
}

/**
 * Sets the boolean that determines whether to show the Sea Pen introduction
 * dialog.
 */
export function setShouldShowSeaPenIntroductionDialogAction(
    shouldShowDialog: boolean): SetShouldShowSeaPenIntroductionDialogAction {
  assert(typeof shouldShowDialog === 'boolean');
  return {
    name: SeaPenActionName.SET_SHOULD_SHOW_SEA_PEN_INTRODUCTION_DIALOG,
    shouldShowDialog,
  };
}

export interface DismissSeaPenErrorAction extends Action {
  name: SeaPenActionName.DISMISS_SEA_PEN_ERROR_ACTION;
}

export function dismissSeaPenErrorAction(): DismissSeaPenErrorAction {
  return {name: SeaPenActionName.DISMISS_SEA_PEN_ERROR_ACTION};
}

export interface SetSeaPenFullscreenStateAction extends Action {
  name: SeaPenActionName.SET_SEA_PEN_FULLSCREEN_STATE;
  state: FullscreenPreviewState;
}

/**
 * Enables/disables the fullscreen preview mode for wallpaper.
 */
export function setSeaPenFullscreenStateAction(state: FullscreenPreviewState):
    SetSeaPenFullscreenStateAction {
  return {name: SeaPenActionName.SET_SEA_PEN_FULLSCREEN_STATE, state};
}
