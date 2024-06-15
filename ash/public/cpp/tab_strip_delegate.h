// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TAB_STRIP_DELEGATE_H_
#define ASH_PUBLIC_CPP_TAB_STRIP_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace aura {
class Window;
}

namespace ash {

// Basic tab info.
struct ASH_PUBLIC_EXPORT TabInfo {
  TabInfo();
  TabInfo(const TabInfo& other);
  ~TabInfo();
  std::u16string title;
  GURL url;
  ui::ImageModel favicon;
  base::TimeTicks last_access_timetick;
};

// This delegate is owned by Shell and used by //ash/webui/boca_ui to fetch
// opened tabs in browser windows.
class ASH_PUBLIC_EXPORT TabStripDelegate {
 public:
  virtual ~TabStripDelegate() = default;

  // Gathers the tab list information for the window.
  virtual std::vector<TabInfo> GetTabsListForWindow(
      aura::Window* window) const = 0;
};

}  // namespace ash
#endif  // ASH_PUBLIC_CPP_TAB_STRIP_DELEGATE_H_
