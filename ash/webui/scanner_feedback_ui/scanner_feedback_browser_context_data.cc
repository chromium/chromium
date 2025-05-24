// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanner_feedback_ui/scanner_feedback_browser_context_data.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/unguessable_token.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {

const void* const kBrowserContextKey = &kBrowserContextKey;

struct BrowserContextData : public base::SupportsUserData::Data {
  std::map<base::UnguessableToken, ScannerFeedbackInfo> feedback_info_map;

  static BrowserContextData* GetForBrowserContext(
      content::BrowserContext& browser_context LIFETIME_BOUND) {
    base::SupportsUserData::Data* data =
        browser_context.GetUserData(&kBrowserContextKey);

    // Can be nullptr.
    return static_cast<BrowserContextData*>(data);
  }

  static BrowserContextData& CreateOrGetForBrowserContext(
      content::BrowserContext& browser_context LIFETIME_BOUND) {
    BrowserContextData* current_data = GetForBrowserContext(browser_context);

    if (current_data != nullptr) {
      return *current_data;
    }

    auto new_data = std::make_unique<BrowserContextData>();
    BrowserContextData& new_data_ref = *new_data;
    browser_context.SetUserData(&kBrowserContextKey, std::move(new_data));
    // Still valid, as the `unique_ptr` - now owned by the browser context -
    // still points to the same heap-allocated memory.
    return new_data_ref;
  }
};

}  // namespace

base::ScopedClosureRunner SetScannerFeedbackInfoForBrowserContext(
    content::BrowserContext& browser_context,
    base::UnguessableToken id,
    ScannerFeedbackInfo feedback_info) {
  auto& data =
      BrowserContextData::CreateOrGetForBrowserContext(browser_context);
  data.feedback_info_map.insert({id, std::move(feedback_info)});

  return base::ScopedClosureRunner(base::BindOnce(
      [](base::WeakPtr<content::BrowserContext> weak_browser_context,
         base::UnguessableToken id) {
        if (weak_browser_context == nullptr) {
          // The browser context is already gone - no need to clean up.
          return;
        }

        auto* data =
            BrowserContextData::GetForBrowserContext(*weak_browser_context);
        if (data == nullptr) {
          // The browser context's user data was cleared.
          return;
        }

        data->feedback_info_map.erase(id);
      },
      browser_context.GetWeakPtr(), id));
}

ScannerFeedbackInfo* GetScannerFeedbackInfoForBrowserContext(
    content::BrowserContext& browser_context LIFETIME_BOUND,
    base::UnguessableToken id) {
  auto* data = BrowserContextData::GetForBrowserContext(browser_context);
  if (data == nullptr) {
    return nullptr;
  }

  auto it = data->feedback_info_map.find(id);
  if (it == data->feedback_info_map.end()) {
    return nullptr;
  }

  return &it->second;
}

std::optional<ScannerFeedbackInfo> TakeScannerFeedbackInfoForBrowserContext(
    content::BrowserContext& browser_context,
    base::UnguessableToken id) {
  auto* data = BrowserContextData::GetForBrowserContext(browser_context);
  if (data == nullptr) {
    return std::nullopt;
  }

  auto it = data->feedback_info_map.find(id);
  if (it == data->feedback_info_map.end()) {
    return std::nullopt;
  }

  std::optional<ScannerFeedbackInfo> feedback_info = std::move(it->second);
  data->feedback_info_map.erase(it);

  return feedback_info;
}

}  // namespace ash
