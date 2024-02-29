// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenImageId} from './constants.js';
import {MantaStatusCode, RecentSeaPenThumbnailData, SeaPenThumbnail} from './sea_pen.mojom-webui.js';

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
  currentSelected: SeaPenImageId|null;
  pendingSelected: SeaPenImageId|SeaPenThumbnail|null;
  thumbnailResponseStatusCode: MantaStatusCode|null;
  shouldShowSeaPenTermsOfServiceDialog: boolean;
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
    currentSelected: null,
    pendingSelected: null,
    shouldShowSeaPenTermsOfServiceDialog: false,
  };
}
