// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/detached_web_contents.h"

#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kWebContentsWidth = 1024;
constexpr int kWebContentsHeight = 768 * 2;

std::unique_ptr<content::WebContents> CreateWebContents(Profile* profile,
                                                        const GURL& url) {
  scoped_refptr<content::SiteInstance> initial_site_instance_for_new_contents =
      content::SiteInstance::CreateForURL(profile, url);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          profile, initial_site_instance_for_new_contents));

  web_contents->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
  // Provide more height so that the change password button is visible on
  // screen.
  web_contents->Resize({kWebContentsWidth, kWebContentsHeight});
  return web_contents;
}

std::unique_ptr<views::Widget> CreateWidgetAndAttachWebContents(
    Profile* profile,
    content::WebContents* web_contents) {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams init_params(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.bounds = gfx::Rect(0, 0, kWebContentsWidth, kWebContentsHeight);
  init_params.name = "PasswordChangeWidget";

  widget->Init(std::move(init_params));

  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(profile);
  web_view->SetWebContents(web_contents);

  widget->SetContentsView(std::move(web_view));

  return widget;
}

}  // namespace

DetachedWebContents::DetachedWebContents(Profile* profile, const GURL& url)
    : web_contents_(CreateWebContents(profile, url)) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kUseDetachedWidget)) {
    widget_ = CreateWidgetAndAttachWebContents(profile, GetWebContents());
    closure_runner_ = GetWebContents()->IncrementCapturerCount(
        {kWebContentsWidth, kWebContentsHeight}, /*stay_hidden=*/false,
        /*stay_awake=*/true, /*is_activity=*/true);
  }
}

DetachedWebContents::~DetachedWebContents() = default;

// static
std::unique_ptr<content::WebContents> DetachedWebContents::ReleaseWebContents(
    std::unique_ptr<DetachedWebContents> detached_web_contents) {
  CHECK(detached_web_contents);
  std::unique_ptr<content::WebContents> web_contents =
      std::move(detached_web_contents->web_contents_);
  // Destroy DetachedWebContents to release `closure_runner_` before returning
  // the web_contents;
  detached_web_contents.reset();
  return web_contents;
}

content::WebContents* DetachedWebContents::GetWebContents() {
  return web_contents_.get();
}
