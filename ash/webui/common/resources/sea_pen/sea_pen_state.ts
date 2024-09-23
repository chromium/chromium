// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenImageId} from './constants.js';
import {MantaStatusCode, RecentSeaPenThumbnailData, SeaPenQuery, SeaPenThumbnail, TextQueryHistoryEntry} from './sea_pen.mojom-webui.js';

export interface SeaPenLoadingState {
  recentImageData: Record<SeaPenImageId, boolean>;
  recentImages: boolean;
  thumbnails: boolean;
  currentSelected: boolean;
  setImage: number;
}

export interface SeaPenState {
  loading: SeaPenLoadingState;
  recentImageData: Record<SeaPenImageId, RecentSeaPenThumbnailData|null>;
  recentImages: SeaPenImageId[]|null;
  thumbnails: SeaPenThumbnail[]|null;
  currentSeaPenQuery: SeaPenQuery|null;
  currentSelected: SeaPenImageId|null;
  pendingSelected: SeaPenImageId|SeaPenThumbnail|null;
  thumbnailResponseStatusCode: MantaStatusCode|null;
  shouldShowSeaPenIntroductionDialog: boolean;
  error: string|null;
  textQueryHistory: TextQueryHistoryEntry[]|null;
}

export function emptyState(): SeaPenState {
  return {
    loading: {
      recentImages: false,
      recentImageData: {},
      thumbnails: false,
      currentSelected: false,
      setImage: 0,
    },
    recentImageData: {},
    recentImages: null,
    thumbnailResponseStatusCode: null,
    thumbnails: null,
    currentSeaPenQuery: null,
    currentSelected: null,
    pendingSelected: null,
    shouldShowSeaPenIntroductionDialog: false,
    error: null,
    textQueryHistory: null,
  };
}
