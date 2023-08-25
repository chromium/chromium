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
    case kFeedbackSourceAutofillContextMenu:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosAutofillContextMenu;
    case kFeedbackSourceSadTabPage:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosSadTabPage;
    case kFeedbackSourceChromeLabs:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosChromeLabs;
    case kFeedbackSourceQuickAnswers:
      return crosapi::mojom::LacrosFeedbackSource::kLacrosQuickAnswers;
    case kFeedbackSourceWindowLayoutMenu:
      return crosapi::mojom::LacrosFeedbackSource::
          kDeprecatedLacrosWindowLayoutMenu;
    case kFeedbackSourceCookieControls:
      return crosapi::mojom::LacrosFeedbackSource::
          kFeedbackSourceCookieControls;
    case kFeedbackSourceSettingsPerformancePage:
      return crosapi::mojom::LacrosFeedbackSource::
          kFeedbackSourceSettingsPerformancePage;
    case kFeedbackSourceProfileErrorDialog:
      return crosapi::mojom::LacrosFeedbackSource::
          kFeedbackSourceProfileErrorDialog;
    case kFeedbackSourceQuickOffice:
      return crosapi::mojom::LacrosFeedbackSource::kFeedbackSourceQuickOffice;
    default:
      LOG(ERROR) << "ShowFeedbackPage is called by unknown Lacros source: "
                 << static_cast<int>(source);
      NOTREACHED();
      return crosapi::mojom::LacrosFeedbackSource::kUnknown;
  }
}

crosapi::mojom::FeedbackInfoPtr ToMojoFeedbackInfo(
    const GURL& page_url,
    FeedbackSource source,
    const std::string& description_template,
    const std::string& description_placeholder_text,
    const std::string& category_tag,
    const std::string& extra_diagnostics,
    base::Value::Dict autofill_metadata) {
  auto mojo_feedback = crosapi::mojom::FeedbackInfo::New();
  mojo_feedback->page_url = page_url;
  mojo_feedback->source = ToMojoLacrosFeedbackSource(source);
  mojo_feedback->description_template = description_template;
  mojo_feedback->description_placeholder_text = description_placeholder_text;
  mojo_feedback->category_tag = category_tag;
  mojo_feedback->extra_diagnostics = extra_diagnostics;
  mojo_feedback->autofill_metadata = base::Value(std::move(autofill_metadata));
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
                            const std::string& extra_diagnostics,
                            base::Value::Dict autofill_metadata) {
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::Feedback>()
      ->ShowFeedbackPage(ToMojoFeedbackInfo(
          page_url, source, description_template, description_placeholder_text,
          category_tag, extra_diagnostics, std::move(autofill_metadata)));
}

}  // namespace internal
}  // namespace chrome
