// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_tracing_delegate.h"

#include <memory>

#include "base/notreached.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace android_webview {

AwTracingDelegate::AwTracingDelegate() {}
AwTracingDelegate::~AwTracingDelegate() {}

bool AwTracingDelegate::IsAllowedToBeginBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  // Background tracing is allowed in general and can be restricted when
  // configuring BackgroundTracingManager.
  return true;
}

bool AwTracingDelegate::IsAllowedToEndBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data,
    bool is_crash_scenario) {
  // Background tracing is allowed in general and can be restricted when
  // configuring BackgroundTracingManager.
  return true;
}

absl::optional<base::Value> AwTracingDelegate::GenerateMetadataDict() {
  base::Value metadata_dict(base::Value::Type::DICTIONARY);
  metadata_dict.SetStringKey("revision", version_info::GetLastChange());
  return metadata_dict;
}

}  // namespace android_webview
