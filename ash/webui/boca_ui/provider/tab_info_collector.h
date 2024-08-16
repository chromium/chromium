// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_PROVIDER_TAB_INFO_COLLECTOR_H_
#define ASH_WEBUI_BOCA_UI_PROVIDER_TAB_INFO_COLLECTOR_H_

#include <memory>

#include "ash/public/cpp/tab_strip_delegate.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace mojom = ash::boca::mojom;

using GetWindowsTabsListCallback =
    base::OnceCallback<void(std::vector<mojom::WindowPtr>)>;

namespace ui {
class ImageModel;
}

namespace ash::boca {
class TabInfoCollector {
 public:
  class ImageGenerator {
   public:
    ImageGenerator();
    explicit ImageGenerator(content::WebUI* web_ui);
    ImageGenerator(const ImageGenerator&) = delete;
    ImageGenerator& operator=(const ImageGenerator) = delete;
    virtual ~ImageGenerator();

    virtual std::string StringifyImage(ui::ImageModel image);

   private:
    raw_ptr<content::WebUI> web_ui_;
  };

  explicit TabInfoCollector(content::WebUI* web_ui);
  explicit TabInfoCollector(
      std::unique_ptr<TabInfoCollector::ImageGenerator> image_generator);
  TabInfoCollector(const TabInfoCollector&) = delete;
  TabInfoCollector& operator=(const TabInfoCollector&) = delete;
  ~TabInfoCollector();
  void GetWindowTabInfo(GetWindowsTabsListCallback callback);

 private:
  raw_ptr<content::WebUI> web_ui_;
  std::unique_ptr<ImageGenerator> image_generator_;

  mojom::TabInfoPtr AshToPageTabInfo(ash::TabInfo tab);
  void SortWindowList(std::vector<std::vector<ash::TabInfo>>& windows_list);
  std::vector<mojom::WindowPtr> AshToPageWindows(
      std::vector<std::vector<ash::TabInfo>> windows);
};

}  // namespace ash::boca
#endif  // ASH_WEBUI_BOCA_UI_PROVIDER_TAB_INFO_COLLECTOR_H_
