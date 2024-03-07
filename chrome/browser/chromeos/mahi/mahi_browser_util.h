// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_BROWSER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_BROWSER_UTIL_H_

#include <optional>
#include <string>

#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace mahi {

using ActionType = crosapi::mojom::MahiContextMenuActionType;

// Contains the types of button existed in Mahi Menu.
enum class ButtonType {
  kSummary = 0,
  kOutline = 1,
  kSettings = 2,
  kQA = 3,
  kMaxValue = kQA,
};

ActionType MatchButtonTypeToActionType(const ButtonType button_type);

// State struct to keep the information of a web content.
struct WebContentState {
  base::UnguessableToken page_id = base::UnguessableToken::Create();
  GURL url;
  std::u16string title;
  gfx::ImageSkia favicon = gfx::ImageSkia();
  std::optional<bool> is_distillable = std::nullopt;

  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  ui::AXTreeUpdate snapshot;

  WebContentState(const WebContentState& state);
  WebContentState& operator=(const WebContentState& state) = default;

  WebContentState(const GURL& url, const std::u16string& title);
  ~WebContentState();
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_BROWSER_UTIL_H_
