// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

// Returns true if `maybeDataUrl` is a Url that contains a base64 encoded image.
export function isImageDataUrl(maybeDataUrl: unknown): maybeDataUrl is Url {
  return !!maybeDataUrl && typeof maybeDataUrl === 'object' &&
      'url' in maybeDataUrl && typeof maybeDataUrl.url === 'string' &&
      (maybeDataUrl.url.startsWith('data:image/png;base64') ||
       maybeDataUrl.url.startsWith('data:image/jpeg;base64'));
}

// Returns true if `maybeArray` is an array with at least one item.
export function isNonEmptyArray(maybeArray: unknown): maybeArray is unknown[] {
  return Array.isArray(maybeArray) && maybeArray.length > 0;
}

// Returns true is `obj` is a FilePath with a non-empty path.
export function isNonEmptyFilePath(obj: unknown): obj is FilePath {
  return !!obj && typeof obj === 'object' && 'path' in obj &&
      typeof obj.path === 'string' && !!obj.path;
}
