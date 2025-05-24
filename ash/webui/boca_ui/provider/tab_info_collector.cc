// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/tab_info_collector.h"

#include <memory>

#include "ash/public/cpp/tab_strip_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/base64.h"
#include "base/check_is_test.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/full_restore_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "skia/ext/codec_utils.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/wm/core/window_util.h"

namespace ash::boca {

TabInfoCollector::TabInfoCollector(content::WebUI* web_ui, bool is_producer)
    : is_producer_(is_producer), web_ui_(web_ui) {}

TabInfoCollector::TabInfoCollector(bool is_producer)
    : is_producer_(is_producer) {}
TabInfoCollector::~TabInfoCollector() = default;

void TabInfoCollector::GetWindowTabInfo(GetWindowsTabsListCallback callback) {
  if (!is_producer_) {
    GetWindowTabInfoForTarget(
        web_ui_->GetWebContents()->GetTopLevelNativeWindow(),
        std::move(callback));
    return;
  }
  GetWindowTabInfoForAllBrowserWindows(std::move(callback));
}

void TabInfoCollector::GetWindowTabInfoForTarget(
    aura::Window* target_window,
    GetWindowsTabsListCallback callback) {
  auto* delegate = Shell::Get()->tab_strip_delegate();
  std::vector<std::vector<ash::TabInfo>> windows = {
      delegate->GetTabsListForWindow(target_window)};
  std::move(callback).Run(AshToPageWindows(windows));
}

void TabInfoCollector::GetWindowTabInfoForAllBrowserWindows(
    GetWindowsTabsListCallback callback) {
  auto* const shell = Shell::Get();
  auto mru_windows =
      shell->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  auto* delegate = shell->tab_strip_delegate();

  std::vector<std::vector<ash::TabInfo>> windows;
  for (aura::Window* window : mru_windows) {
    // skip transient windows.
    if (wm::GetTransientParent(window)) {
      continue;
    }
    // Only load browser window.
    if (full_restore::GetAppId(window) != app_constants::kChromeAppId) {
      continue;
    }
    auto window_tabs = delegate->GetTabsListForWindow(window);
    if (window_tabs.size()) {
      // TODO-crbug.com/355508827:Set user selected window name.
      windows.push_back(window_tabs);
    }
  }
  SortWindowList(windows);
  std::move(callback).Run(AshToPageWindows(windows));
}

mojom::TabInfoPtr TabInfoCollector::AshToPageTabInfo(ash::TabInfo tab) {
  mojom::TabInfoPtr tab_info = mojom::TabInfo::New();
  tab_info->title = base::UTF16ToUTF8(tab.title);
  tab_info->url = std::move(tab.url);
  tab_info->favicon = std::move(tab.favicon);
  tab_info->id = tab.id;
  return tab_info;
}

void TabInfoCollector::SortWindowList(
    std::vector<std::vector<ash::TabInfo>>& windows_list) {
  for (std::vector<ash::TabInfo>& window : windows_list) {
    // Sort tab on non-ascending order of last access time.
    std::ranges::sort(window, [](const ash::TabInfo& a, const ash::TabInfo& b) {
      return a.last_access_timetick > b.last_access_timetick;
    });
  }

  // Sort window on non-ascending order of last access time.
  std::ranges::sort(windows_list, [](const std::vector<ash::TabInfo>& a,
                                     const std::vector<ash::TabInfo>& b) {
    return a[0].last_access_timetick > b[0].last_access_timetick;
  });
}

std::vector<mojom::WindowPtr> TabInfoCollector::AshToPageWindows(
    std::vector<std::vector<ash::TabInfo>> windows) {
  std::vector<mojom::WindowPtr> out;
  for (auto window : windows) {
    mojom::WindowPtr window_out = mojom::Window::New();
    for (auto tab : window) {
      window_out->tab_list.push_back(AshToPageTabInfo(tab));
    }
    out.push_back(std::move(window_out));
  }
  return out;
}

}  // namespace ash::boca
