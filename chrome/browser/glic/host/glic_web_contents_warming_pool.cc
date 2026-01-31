// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicWebContentsWarmingPool::GlicWebContentsWarmingPool(Profile* profile)
    : profile_(profile) {}

GlicWebContentsWarmingPool::~GlicWebContentsWarmingPool() = default;

std::unique_ptr<WebUIContentsContainer>
GlicWebContentsWarmingPool::TakeContainer() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicWebContentsWarming));
  EnsurePreload();
  std::unique_ptr<WebUIContentsContainer> container =
      std::move(warmed_container_);
  EnsurePreload();
  return container;
}

void GlicWebContentsWarmingPool::EnsurePreload() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicWebContentsWarming));
  if (warmed_container_ && warmed_container_->web_contents()->IsCrashed()) {
    warmed_container_.reset();
  }
  if (warmed_container_) {
    return;
  }
  warmed_container_ = std::make_unique<WebUIContentsContainer>(
      profile_, /*initially_hidden=*/false);
}

void GlicWebContentsWarmingPool::Shutdown() {
  warmed_container_.reset();
}
}  // namespace glic
