// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/fre_webui_contents_container.h"

#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"

namespace glic {

FreWebUIContentsContainer::FreWebUIContentsContainer(
    Profile* profile,
    views::WebView* web_view,
    GlicFreController* fre_controller)
    : web_contents_(content::WebContents::Create(
          content::WebContents::CreateParams(profile))),
      fre_web_view_(web_view),
      fre_controller_(fre_controller) {
  CHECK(web_contents_);
  web_contents_->SetDelegate(this);
  web_contents_->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL{chrome::kChromeUIGlicFreURL}));
}

FreWebUIContentsContainer::~FreWebUIContentsContainer() {
  web_contents_->ClosePage();
}

void FreWebUIContentsContainer::SetContentsBounds(content::WebContents* source,
                                                  const gfx::Rect& bounds) {
  gfx::Size new_size = bounds.size();
  fre_web_view_->SetPreferredSize(new_size);
  fre_web_view_->SizeToPreferredSize();
  fre_controller_->UpdateFreWidgetSize(new_size);
}

}  // namespace glic
