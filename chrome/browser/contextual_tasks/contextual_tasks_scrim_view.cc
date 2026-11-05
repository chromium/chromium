// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_scrim_view.h"

#include <string>

#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksScrimView::ContextualTasksScrimView(
    content::BrowserContext* browser_context)
    : views::WebView(browser_context) {
  SetProperty(views::kElementIdentifierKey, kContextualTasksScrimViewElementId);
}

ContextualTasksScrimView::~ContextualTasksScrimView() = default;

void ContextualTasksScrimView::SetWebContents(
    content::WebContents* web_contents) {
  is_showing_error_page_ =
      web_contents && web_contents->GetVisibleURL().ref() == "error";
  views::WebView::SetWebContents(web_contents);
}

void ContextualTasksScrimView::SetShowErrorPage(bool show_error_page) {
  if (is_showing_error_page_ == show_error_page || !web_contents()) {
    return;
  }
  is_showing_error_page_ = show_error_page;
  const GURL target_url(
      base::StrCat({chrome::kChromeUIContextualTasksGhostLoaderURL,
                    show_error_page ? "#error" : "#ghost-loader"}));
  web_contents()->GetController().LoadURL(target_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                          std::string());
}

BEGIN_METADATA(ContextualTasksScrimView)
END_METADATA

}  // namespace contextual_tasks
