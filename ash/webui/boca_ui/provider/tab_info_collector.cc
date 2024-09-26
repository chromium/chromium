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
#include "components/app_constants/constants.h"
#include "components/app_restore/full_restore_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/gfx/image/buffer_w_stream.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/wm/core/window_util.h"

namespace {
// Web UI image helper copied from //chrome/browser/ui/webui/util/image_util.cc
// due to dependency constraint.
std::string MakeDataURIForImage(base::span<const uint8_t> image_data,
                                std::string_view mime_subtype) {
  std::string result = "data:image/";
  result.append(mime_subtype.begin(), mime_subtype.end());
  result += ";base64,";
  result += base::Base64Encode(image_data);
  return result;
}

std::string EncodePNGAndMakeDataURI(gfx::ImageSkia image, float scale_factor) {
  const SkBitmap& bitmap = image.GetRepresentation(scale_factor).GetBitmap();
  gfx::BufferWStream stream;
  const bool encoding_succeeded =
      SkPngEncoder::Encode(&stream, bitmap.pixmap(), {});
  DCHECK(encoding_succeeded);
  return MakeDataURIForImage(
      base::as_bytes(base::make_span(stream.TakeBuffer())), "png");
}
// End of image lib

}  // namespace

namespace ash::boca {
TabInfoCollector::ImageGenerator::ImageGenerator(content::WebUI* web_ui)
    : web_ui_(web_ui) {}
TabInfoCollector::ImageGenerator::ImageGenerator() = default;
TabInfoCollector::ImageGenerator::~ImageGenerator() = default;

std::string TabInfoCollector::ImageGenerator::StringifyImage(
    ui::ImageModel image) {
  // For test only.
  if (!web_ui_) {
    CHECK_IS_TEST();
    return "";
  }
  const ui::ColorProvider& provider =
      web_ui_->GetWebContents()->GetColorProvider();
  gfx::ImageSkia raster_favicon = image.Rasterize(&provider);
  return EncodePNGAndMakeDataURI(raster_favicon,
                                 web_ui_->GetDeviceScaleFactor());
}

TabInfoCollector::TabInfoCollector(content::WebUI* web_ui) {
  image_generator_ = std::make_unique<ImageGenerator>(web_ui);
}

TabInfoCollector::TabInfoCollector(
    std::unique_ptr<TabInfoCollector::ImageGenerator> image_generator)
    : image_generator_(std::move(image_generator)) {}
TabInfoCollector::~TabInfoCollector() = default;

void TabInfoCollector::GetWindowTabInfo(GetWindowsTabsListCallback callback) {
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
    // TODO(b/355508827):Set user selected window name.
    windows.push_back(delegate->GetTabsListForWindow(window));
  }
  SortWindowList(windows);
  std::move(callback).Run(AshToPageWindows(windows));
}

mojom::TabInfoPtr TabInfoCollector::AshToPageTabInfo(ash::TabInfo tab) {
  mojom::TabInfoPtr tab_info = mojom::TabInfo::New();
  tab_info->title = base::UTF16ToUTF8(tab.title);
  tab_info->url = tab.url;
  tab_info->favicon = image_generator_->StringifyImage(tab.favicon);
  return tab_info;
}

void TabInfoCollector::SortWindowList(
    std::vector<std::vector<ash::TabInfo>>& windows_list) {
  for (std::vector<ash::TabInfo>& window : windows_list) {
    // Sort tab on non-ascending order of last access time.
    base::ranges::sort(window,
                       [](const ash::TabInfo& a, const ash::TabInfo& b) {
                         return a.last_access_timetick > b.last_access_timetick;
                       });
  }

  // Sort window on non-ascending order of last access time.
  base::ranges::sort(windows_list, [](const std::vector<ash::TabInfo>& a,
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
