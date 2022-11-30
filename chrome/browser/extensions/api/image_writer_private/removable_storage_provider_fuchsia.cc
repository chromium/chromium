// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"

#include "base/notreached.h"

namespace extensions {

// static
scoped_refptr<StorageDeviceList>
RemovableStorageProvider::PopulateDeviceList() {
  // TODO(crbug.com/1233550): Integrate once platform APIs exist.
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

}  // namespace extensions
