// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_BROWSER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_BROWSER_UTIL_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr_exclusion.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace mahi {

// State struct to keep the information of a web content.
struct WebContentState {
  base::UnguessableToken page_id = base::UnguessableToken::Create();
  GURL url;
  std::u16string title;
  gfx::ImageSkia favicon = gfx::ImageSkia();
  std::optional<bool> is_distillable = std::nullopt;
  bool is_incognito = true;

  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  ui::AXTreeUpdate snapshot;
  // RAW_PTR_EXCLUSION: We only use its address as an identifier and never
  // dereference it.
  RAW_PTR_EXCLUSION aura::Window* top_level_native_window = nullptr;

  WebContentState(const WebContentState& state);
  WebContentState& operator=(const WebContentState& state) = default;

  WebContentState(const GURL& url, const std::u16string& title);
  ~WebContentState();
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_BROWSER_UTIL_H_
