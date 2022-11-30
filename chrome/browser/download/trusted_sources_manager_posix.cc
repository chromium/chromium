// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/trusted_sources_manager.h"

#include "base/memory/ptr_util.h"

// static
std::unique_ptr<TrustedSourcesManager> TrustedSourcesManager::Create() {
  return base::WrapUnique(new TrustedSourcesManager);
}
