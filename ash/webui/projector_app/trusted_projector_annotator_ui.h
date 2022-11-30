// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_TRUSTED_PROJECTOR_ANNOTATOR_UI_H_
#define ASH_WEBUI_PROJECTOR_APP_TRUSTED_PROJECTOR_ANNOTATOR_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class GURL;
class PrefService;

namespace ash {

// The implementation for the Projector annotator for screen recording
// annotations.
class TrustedProjectorAnnotatorUI : public ui::MojoBubbleWebUIController {
 public:
  TrustedProjectorAnnotatorUI(content::WebUI* web_ui,
                              const GURL& url,
                              PrefService* pref_service);
  ~TrustedProjectorAnnotatorUI() override;
  TrustedProjectorAnnotatorUI(const TrustedProjectorAnnotatorUI&) = delete;
  TrustedProjectorAnnotatorUI& operator=(const TrustedProjectorAnnotatorUI&) =
      delete;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_TRUSTED_PROJECTOR_ANNOTATOR_UI_H_
