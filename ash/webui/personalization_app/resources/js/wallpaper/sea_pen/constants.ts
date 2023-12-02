// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

/**
 * An interface for the data of a recent sea pen image.
 */
export interface RecentSeaPenData {
  url: Url;
  queryInfo: string;
}
