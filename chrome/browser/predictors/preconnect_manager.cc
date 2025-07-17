// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/preconnect_manager.h"

#include <memory>

#include "chrome/browser/predictors/preconnect_manager_impl.h"

namespace predictors {

std::unique_ptr<PreconnectManager> PreconnectManager::Create(
    base::WeakPtr<PreconnectManager::Delegate> delegate,
    content::BrowserContext* browser_context) {
  return std::make_unique<PreconnectManagerImpl>(std::move(delegate),
                                                 browser_context);
}

}  // namespace predictors
