// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/headless_page_host.h"

#include <utility>

#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_AURA)
#include "base/check_is_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#endif

namespace actor {

HeadlessPageHost::HeadlessPageHost(content::WebContents* contents,
                                   const gfx::Size& size) {
#if defined(USE_AURA)
  // Without a views environment, e.g. in unit tests, there is nothing to host
  // the page in.
  if (!views::ViewsDelegate::GetInstance()) {
    CHECK_IS_TEST();
    return;
  }
  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::Ownership::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(size);
  params.name = "HeadlessActorPage";
  widget_->Init(std::move(params));
  auto web_view =
      std::make_unique<views::WebView>(contents->GetBrowserContext());
  web_view->SetWebContents(contents);
  widget_->SetContentsView(std::move(web_view));
  capturer_ = contents->IncrementCapturerCount(size, /*stay_hidden=*/false,
                                               /*stay_awake=*/true,
                                               /*is_activity=*/true);
#endif
}

HeadlessPageHost::~HeadlessPageHost() = default;

}  // namespace actor
