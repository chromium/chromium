// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_observer.h"

#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"

namespace policy {

// static
DlpContentObserver* DlpContentObserver::Get() {
  // Initializes DlpContentManager if needed.
  return DlpContentManager::Get();
}

}  // namespace policy
