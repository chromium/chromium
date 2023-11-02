// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace chrome {
namespace internal {

namespace {

crosapi::mojom::LacrosFeedbackSource ToMojoLacrosFeedbackSource(
    FeedbackSource source) {
  switch (source) {
    case kFeedbackSourceBrowserCommand:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosBrowserCommand;
    case kFeedbackSourceMdSettingsAboutPage:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosSettingsAboutPage;
    default:
      NOTREACHED() << "ShowFeedbackPage is called by unknown Lacros source";
      return crosapi::mojom::LacrosFeedbackSource::kLacrosBrowserCommand;
  }
}

crosapi::mojom::FeedbackInfoPtr ToMojoFeedbackInfo(
    const GURL& page_url,
    FeedbackSource source,
    const std::string& description_template,
    const std::string& description_placeholder_text,
    const std::string& category_tag,
    const std::string& extra_diagnostics) {
  auto mojo_feedback = crosapi::mojom::FeedbackInfo::New();
  mojo_feedback->page_url = page_url;
  mojo_feedback->source = ToMojoLacrosFeedbackSource(source);
  mojo_feedback->description_template = description_template;
  mojo_feedback->description_placeholder_text = description_placeholder_text;
  mojo_feedback->category_tag = category_tag;
  mojo_feedback->extra_diagnostics = extra_diagnostics;
  return mojo_feedback;
}

}  //  namespace

// Requests to show Feedback ui remotely in ash via crosapi mojo call.
// Note: This function should only be called from show_feedback_page.cc.
void ShowFeedbackPageLacros(const GURL& page_url,
                            FeedbackSource source,
                            const std::string& description_template,
                            const std::string& description_placeholder_text,
                            const std::string& category_tag,
                            const std::string& extra_diagnostics) {
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::Feedback>()
      ->ShowFeedbackPage(ToMojoFeedbackInfo(
          page_url, source, description_template, description_placeholder_text,
          category_tag, extra_diagnostics));
}

}  // namespace internal
}  // namespace chrome
