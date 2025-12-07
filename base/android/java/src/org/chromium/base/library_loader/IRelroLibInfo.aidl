// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.base.library_loader;

parcelable IRelroLibInfo {
  String libFilePath;
  long loadAddress;
  long loadSize;
  long relroStart;
  long relroSize;
  @nullable ParcelFileDescriptor fd;
}
