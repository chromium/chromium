// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MALL_CHROME_MALL_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_MALL_CHROME_MALL_UI_DELEGATE_H_

#include <string_view>

#include "ash/webui/mall/mall_ui_delegate.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "content/public/browser/web_ui.h"
namespace ash {

class ChromeMallUIDelegate : public MallUIDelegate {
 public:
  explicit ChromeMallUIDelegate(content::WebUI* web_ui);
  ~ChromeMallUIDelegate() override;

  void GetMallEmbedUrl(std::string_view path,
                       base::OnceCallback<void(const GURL&)> callback) override;

 private:
  raw_ptr<content::WebUI> web_ui_;  // Owns `this`.
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MALL_CHROME_MALL_UI_DELEGATE_H_
