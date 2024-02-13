// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {RecentSeaPenData} from './constants.js';
import {MantaStatusCode, SeaPenThumbnail} from './sea_pen.mojom-webui.js';
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
    case SeaPenActionName.BEGIN_SELECT_RECENT_SEA_PEN_IMAGE:
      return {
        ...state,
        setImage: state.setImage + 1,
      };
    case SeaPenActionName.END_SELECT_RECENT_SEA_PEN_IMAGE:
    case SeaPenActionName.END_SELECT_SEA_PEN_THUMBNAIL:
      if (state.setImage <= 0) {
        console.error('Impossible state for loading.setImage');
        // Reset to 0.
        return {
          ...state,
          setImage: 0,
        };
      }
      return {
        ...state,
        setImage: state.setImage - 1,
      };
    case SeaPenActionName.BEGIN_LOAD_SELECTED_RECENT_SEA_PEN_IMAGE:
      return {
        ...state,
        currentSelected: true,
      };
    case SeaPenActionName.BEGIN_SELECT_SEA_PEN_THUMBNAIL:
      return {
        ...state,
        currentSelected: true,
        setImage: state.setImage + 1,
      };
    case SeaPenActionName.SET_SELECTED_RECENT_SEA_PEN_IMAGE:
      return {
        ...state,
        currentSelected: false,
      };
    default:
      return state;
  }
}

function thumbnailResponseStatusCodeReducer(
    state: MantaStatusCode|null, action: SeaPenActions): MantaStatusCode|null {
  switch (action.name) {
    case SeaPenActionName.SET_THUMBNAIL_RESPONSE_STATUS_CODE:
      return action.thumbnailResponseStatusCode;
    default:
      return state;
  }
}

function currentSelectedReducer(
    state: string|null, action: SeaPenActions): string|null {
  switch (action.name) {
    case SeaPenActionName.SET_SELECTED_RECENT_SEA_PEN_IMAGE:
      return action.key;
    default:
      return state;
  }
}

/**
 * Reducer for the pending selected image. The pendingSelected state is set when
 * a user clicks on an image and before the client code is reached.
 *
 * Note: We allow multiple concurrent requests of selecting images while only
 * keeping the latest pending image and failing others occurred in between.
 * The pendingSelected state should not be cleared in this scenario (of multiple
 * concurrent requests). Otherwise, it results in a unwanted jumpy motion of
 * selected state.
 */
function pendingSelectedReducer(
    state: FilePath|SeaPenThumbnail|null, action: SeaPenActions,
    globalState: SeaPenState): FilePath|SeaPenThumbnail|null {
  switch (action.name) {
    case SeaPenActionName.BEGIN_SELECT_RECENT_SEA_PEN_IMAGE:
      return action.image;
    case SeaPenActionName.SET_SELECTED_RECENT_SEA_PEN_IMAGE:
      const {key} = action;
      if (state && !key) {
        console.warn('pendingSelectedReducer: Failed to get selected image.');
        return null;
      } else if (globalState.loading.setImage == 0) {
        // Clear the pending state when there are no more requests.
        return null;
      }
      return state;
    case SeaPenActionName.END_SELECT_RECENT_SEA_PEN_IMAGE:
    case SeaPenActionName.END_SELECT_SEA_PEN_THUMBNAIL:
      const {success} = action;
      if (!success && globalState.loading.setImage <= 1) {
        // Clear the pending selected state if an error occurs and
        // there are no multiple concurrent requests of selecting images.
        return null;
      }
      return state;
    case SeaPenActionName.BEGIN_SELECT_SEA_PEN_THUMBNAIL:
      return action.thumbnail;
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
    case SeaPenActionName.CLEAR_SEA_PEN_THUMBNAILS:
      return null;
    default:
      return state;
  }
}

function shouldShowSeaPenTermsOfServiceDialogReducer(
    state: boolean, action: SeaPenActions): boolean {
  switch (action.name) {
    case SeaPenActionName.SET_SHOULD_SHOW_SEA_PEN_TERMS_OF_SERVICE_DIALOG:
      return action.shouldShowDialog;
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
    thumbnailResponseStatusCode: thumbnailResponseStatusCodeReducer(
        state.thumbnailResponseStatusCode, action),
    thumbnails: thumbnailsReducer(state.thumbnails, action),
    currentSelected: currentSelectedReducer(state.currentSelected, action),
    pendingSelected:
        pendingSelectedReducer(state.pendingSelected, action, state),
    shouldShowSeaPenTermsOfServiceDialog:
        shouldShowSeaPenTermsOfServiceDialogReducer(
            state.shouldShowSeaPenTermsOfServiceDialog, action),
  };
  return newState;
}
