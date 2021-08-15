// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/protocol/forward.h"
#include "chrome/browser/devtools/protocol/page.h"

namespace content {
struct InstallabilityError;
class WebContents;
}

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
