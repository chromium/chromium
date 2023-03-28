// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_ANNOTATOR_UI_H_
#define ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_ANNOTATOR_UI_H_

#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash {

// A delegate used during data source creation to expose some //chrome
// functionality to the data source
class UntrustedProjectorAnnotatorUIDelegate {
 public:
  virtual ~UntrustedProjectorAnnotatorUIDelegate() {}
  // Takes a WebUIDataSource, and populates its load-time data.
  virtual void PopulateLoadTimeData(content::WebUIDataSource* source) = 0;
};

// The webui for chrome-untrusted://projector-annotator.
class UntrustedProjectorAnnotatorUI : public ui::UntrustedWebUIController {
 public:
  // UntrustedProjectorAnnotatorUI does not store the passed in
  // `UntrustedProjectorAnnotatorUIDelegate`.
  UntrustedProjectorAnnotatorUI(
      content::WebUI* web_ui,
      UntrustedProjectorAnnotatorUIDelegate* delegate);
  UntrustedProjectorAnnotatorUI(const UntrustedProjectorAnnotatorUI&) = delete;
  UntrustedProjectorAnnotatorUI& operator=(
      const UntrustedProjectorAnnotatorUI&) = delete;
  ~UntrustedProjectorAnnotatorUI() override;
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_UNTRUSTED_PROJECTOR_ANNOTATOR_UI_H_
