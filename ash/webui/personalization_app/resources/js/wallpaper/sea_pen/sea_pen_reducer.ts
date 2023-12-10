// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {SeaPenThumbnail} from '../../../sea_pen.mojom-webui.js';

import {RecentSeaPenData} from './constants.js';
import {SeaPenActionName, SeaPenActions} from './sea_pen_actions.js';
import {SeaPenLoadingState, SeaPenState} from './sea_pen_state.js';

function loadingReducer(
    state: SeaPenLoadingState, action: SeaPenActions): SeaPenLoadingState {
  switch (action.name) {
    case SeaPenActionName.BEGIN_SEARCH_SEA_PEN_THUMBNAILS:
      return {
        ...state,
        thumbnails: true,
      };
    case SeaPenActionName.SET_SEA_PEN_THUMBNAILS:
      return {
        ...state,
        thumbnails: false,
      };
    case SeaPenActionName.BEGIN_LOAD_RECENT_SEA_PEN_IMAGES:
      return {
        ...state,
        recentImages: true,
      };
    case SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES:
      const newRecentImages: FilePath[] =
          Array.isArray(action.recentImages) ? action.recentImages : [];
      // Only keep loading state for most recent Sea Pen images.
      return {
        ...state,
        recentImageData: newRecentImages.reduce(
            (result, next) => {
              const path = next.path;
              if (state.recentImageData.hasOwnProperty(path)) {
                result[path] = state.recentImageData[path];
              }
              return result;
            },
            {} as Record<FilePath['path'], boolean>),
        // Recent image list is done loading.
        recentImages: false,
      };
    case SeaPenActionName.BEGIN_LOAD_RECENT_SEA_PEN_IMAGE_DATA:
      return {
        ...state,
        recentImageData: {
          ...state.recentImageData,
          [action.id]: true,
        },
      };
    case SeaPenActionName.SET_RECENT_SEA_PEN_IMAGE_DATA:
      return {
        ...state,
        recentImageData: {
          ...state.recentImageData,
          [action.id]: false,
        },
      };
    default:
      return state;
  }
}

function recentImagesReducer(
    state: FilePath[]|null, action: SeaPenActions): FilePath[]|null {
  switch (action.name) {
    case SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES:
      return action.recentImages;
    default:
      return state;
  }
}

function recentImageDataReducer(
    state: Record<FilePath['path'], RecentSeaPenData>,
    action: SeaPenActions): Record<FilePath['path'], RecentSeaPenData> {
  switch (action.name) {
    case SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES:
      const newRecentImages: FilePath[] =
          Array.isArray(action.recentImages) ? action.recentImages : [];
      return newRecentImages.reduce((result, next) => {
        const key = next.path;
        if (key && state.hasOwnProperty(key)) {
          result[key] = state[key];
        }
        return result;
      }, {} as typeof state);
    case SeaPenActionName.SET_RECENT_SEA_PEN_IMAGE_DATA:
      return {...state, [action.id]: action.data};
    default:
      return state;
  }
}

function thumbnailsReducer(
    state: SeaPenThumbnail[]|null, action: SeaPenActions): SeaPenThumbnail[]|
    null {
  switch (action.name) {
    case SeaPenActionName.SET_SEA_PEN_THUMBNAILS:
      assert(!!action.query, 'input text is empty.');
      return action.images;
    default:
      return state;
  }
}

export function seaPenReducer(
    state: SeaPenState, action: SeaPenActions): SeaPenState {
  const newState = {
    loading: loadingReducer(state.loading, action),
    recentImageData: recentImageDataReducer(state.recentImageData, action),
    recentImages: recentImagesReducer(state.recentImages, action),
    thumbnails: thumbnailsReducer(state.thumbnails, action),
  };
  return newState;
}
