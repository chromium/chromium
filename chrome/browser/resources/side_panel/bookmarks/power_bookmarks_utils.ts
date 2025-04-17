// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';

export function getFolderLabel(folder: BookmarksTreeNode|undefined): string {
  if (folder && folder.id !== loadTimeData.getString('otherBookmarksId') &&
    folder.id !== loadTimeData.getString('mobileBookmarksId')) {
    return folder.title;
  } else {
    return loadTimeData.getString('allBookmarks');
  }
}
