// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';

import {SeaPenThumbnail} from '../../../sea_pen.mojom-webui.js';

import {RecentSeaPenData} from './constants.js';

export interface SeaPenState {
  thumbnails: SeaPenThumbnail[]|null;
  thumbnailsLoading: boolean;
  recentImages: FilePath[]|null;
  recentImageData: Record<FilePath['path'], RecentSeaPenData>;
}

export function emptyState(): SeaPenState {
  return {
    thumbnails: null,
    thumbnailsLoading: false,
    recentImages: null,
    recentImageData: {},
  };
}
