// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {SeaPenThumbnail} from '../../../sea_pen.mojom-webui.js';

import {RecentSeaPenData} from './constants.js';

export interface SeaPenLoadingState {
  recentImageData: Record<FilePath['path'], boolean>;
  recentImages: boolean;
  thumbnails: boolean;
}

export interface SeaPenState {
  loading: SeaPenLoadingState;
  recentImageData: Record<FilePath['path'], RecentSeaPenData>;
  recentImages: FilePath[]|null;
  thumbnails: SeaPenThumbnail[]|null;
}

export function emptyState(): SeaPenState {
  return {
    loading: {
      recentImages: false,
      recentImageData: {},
      thumbnails: false,
    },
    recentImageData: {},
    recentImages: null,
    thumbnails: null,
  };
}
