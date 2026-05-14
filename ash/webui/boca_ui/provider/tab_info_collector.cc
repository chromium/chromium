// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/tab_info_collector.h"

#include <memory>
#include <optional>

#include "ash/public/cpp/tab_strip_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
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
namespace {

class TabInfoCollectorImpl : public TabInfoCollector {
 public:
  TabInfoCollectorImpl(content::WebUI* web_ui, bool is_producer)
      : is_producer_(is_producer), web_ui_(web_ui) {}

  explicit TabInfoCollectorImpl(bool is_producer) : is_producer_(is_producer) {}

  TabInfoCollectorImpl(const TabInfoCollectorImpl&) = delete;
  TabInfoCollectorImpl& operator=(const TabInfoCollectorImpl&) = delete;
  ~TabInfoCollectorImpl() override = default;

  // TabInfoCollector:
  std::vector<mojom::WindowPtr> GetWindowTabInfo(
      UrlTypeGetter url_type_getter) override {
    if (!is_producer_) {
      if (web_ui_ && web_ui_->GetWebContents()) {
        return GetWindowTabInfoForTarget(
            web_ui_->GetWebContents()->GetTopLevelNativeWindow(),
            url_type_getter);
      }
      return {};
    }
    return GetWindowTabInfoForAllBrowserWindows(url_type_getter);
  }

  std::vector<mojom::WindowPtr> GetWindowTabInfoForTarget(
      aura::Window* target_window,
      UrlTypeGetter url_type_getter) override {
    if (!Shell::HasInstance()) {
      return {};
    }
    auto* delegate = Shell::Get()->tab_strip_delegate();
    if (!delegate) {
      return {};
    }
    std::vector<std::vector<ash::TabInfo>> windows = {
        delegate->GetTabsListForWindow(target_window)};
    return AshToPageWindows(windows, url_type_getter);
  }

  std::vector<mojom::WindowPtr> GetWindowTabInfoForAllBrowserWindows(
      UrlTypeGetter url_type_getter) override {
    if (!Shell::HasInstance()) {
      return {};
    }
    auto* const shell = Shell::Get();
    auto mru_windows =
        shell->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
    auto* delegate = shell->tab_strip_delegate();
    if (!delegate) {
      return {};
    }

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
    return AshToPageWindows(windows, url_type_getter);
  }

 private:
  mojom::TabInfoPtr AshToPageTabInfo(ash::TabInfo tab,
                                     UrlTypeGetter url_type_getter) {
    mojom::TabInfoPtr tab_info = mojom::TabInfo::New();
    tab_info->title = base::UTF16ToUTF8(tab.title);
    tab_info->url = std::move(tab.url);
    tab_info->favicon = std::move(tab.favicon);
    tab_info->id = tab.id;
    std::optional<mojom::UrlType> url_type = url_type_getter.Run(tab.id);
    if (url_type.has_value()) {
      tab_info->url_type = url_type.value();
    }
    return tab_info;
  }

  void SortWindowList(std::vector<std::vector<ash::TabInfo>>& windows_list) {
    for (std::vector<ash::TabInfo>& window : windows_list) {
      // Sort tab on non-ascending order of last access time.
      std::ranges::sort(
          window, [](const ash::TabInfo& a, const ash::TabInfo& b) {
            return a.last_access_timetick > b.last_access_timetick;
          });
    }

    // Sort window on non-ascending order of last access time.
    std::ranges::sort(windows_list, [](const std::vector<ash::TabInfo>& a,
                                       const std::vector<ash::TabInfo>& b) {
      return a[0].last_access_timetick > b[0].last_access_timetick;
    });
  }

  std::vector<mojom::WindowPtr> AshToPageWindows(
      std::vector<std::vector<ash::TabInfo>> windows,
      UrlTypeGetter url_type_getter) {
    std::vector<mojom::WindowPtr> out;
    for (auto window : windows) {
      mojom::WindowPtr window_out = mojom::Window::New();
      for (auto tab : window) {
        window_out->tab_list.push_back(AshToPageTabInfo(tab, url_type_getter));
      }
      out.push_back(std::move(window_out));
    }
    return out;
  }

  const bool is_producer_;
  const raw_ptr<content::WebUI> web_ui_;
};

}  // namespace

std::unique_ptr<TabInfoCollector> TabInfoCollector::Create(
    content::WebUI* web_ui,
    bool is_producer) {
  return std::make_unique<TabInfoCollectorImpl>(web_ui, is_producer);
}

std::unique_ptr<TabInfoCollector> TabInfoCollector::Create(bool is_producer) {
  return std::make_unique<TabInfoCollectorImpl>(is_producer);
}

}  // namespace ash::boca
