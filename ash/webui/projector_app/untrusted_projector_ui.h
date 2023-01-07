// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_H_
#define ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_H_

#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash {

// A delegate used during data source creation to expose some //chrome
// functionality to the data source
class UntrustedProjectorUIDelegate {
 public:
  // Takes a WebUIDataSource, and populates its load-time data.
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;
};

// The webui for chrome-untrusted://projector.
class UntrustedProjectorUI : public ui::UntrustedWebUIController {
 public:
  UntrustedProjectorUI(content::WebUI* web_ui,
                       UntrustedProjectorUIDelegate* delegate);
  UntrustedProjectorUI(const UntrustedProjectorUI&) = delete;
  UntrustedProjectorUI& operator=(const UntrustedProjectorUI&) = delete;
  ~UntrustedProjectorUI() override;
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_H_
