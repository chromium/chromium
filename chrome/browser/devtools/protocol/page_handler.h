// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/protocol/forward.h"
#include "chrome/browser/devtools/protocol/page.h"

namespace content {
struct InstallabilityError;
class WebContents;
}  // namespace content

class SkBitmap;

class PageHandler : public protocol::Page::Backend {
 public:
  PageHandler(content::WebContents* web_contents,
              protocol::UberDispatcher* dispatcher);
  ~PageHandler() override;

  void ToggleAdBlocking(bool enabled);

  // Page::Backend:
  protocol::Response Enable() override;
  protocol::Response Disable() override;
  protocol::Response SetAdBlockingEnabled(bool enabled) override;
  void GetInstallabilityErrors(
      std::unique_ptr<GetInstallabilityErrorsCallback> callback) override;

  void GetManifestIcons(
      std::unique_ptr<GetManifestIconsCallback> callback) override;

  void PrintToPDF(protocol::Maybe<bool> landscape,
                  protocol::Maybe<bool> display_header_footer,
                  protocol::Maybe<bool> print_background,
                  protocol::Maybe<double> scale,
                  protocol::Maybe<double> paper_width,
                  protocol::Maybe<double> paper_height,
                  protocol::Maybe<double> margin_top,
                  protocol::Maybe<double> margin_bottom,
                  protocol::Maybe<double> margin_left,
                  protocol::Maybe<double> margin_right,
                  protocol::Maybe<protocol::String> page_ranges,
                  protocol::Maybe<bool> ignore_invalid_page_ranges,
                  protocol::Maybe<protocol::String> header_template,
                  protocol::Maybe<protocol::String> footer_template,
                  protocol::Maybe<bool> prefer_css_page_size,
                  protocol::Maybe<protocol::String> transfer_mode,
                  std::unique_ptr<PrintToPDFCallback> callback) override;

 private:
  static void GotInstallabilityErrors(
      std::unique_ptr<GetInstallabilityErrorsCallback> callback,
      std::vector<content::InstallabilityError> installability_errors);

  static void GotManifestIcons(
      std::unique_ptr<GetManifestIconsCallback> callback,
      const SkBitmap* primary_icon);

  base::WeakPtr<content::WebContents> web_contents_;

  bool enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(PageHandler);
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
