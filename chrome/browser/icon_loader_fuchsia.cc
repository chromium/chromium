// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1226242): Implement support for downloads under Fuchsia.

#include "chrome/browser/icon_loader.h"

#include "base/notreached.h"

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  NOTIMPLEMENTED_LOG_ONCE();
  return {};
}

void IconLoader::ReadIcon() {
  NOTIMPLEMENTED_LOG_ONCE();
  delete this;
}
