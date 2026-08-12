// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_ANDROID_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_ANDROID_H_

#include <memory>

#include "chrome/browser/devtools/chrome_devtools_session_base.h"

namespace content {
class DevToolsAgentHostClientChannel;
}  // namespace content

class AutofillHandler;
class BrowserHandlerAndroid;
class TargetHandlerAndroid;

class ChromeDevToolsSessionAndroid : public ChromeDevToolsSessionBase {
 public:
  explicit ChromeDevToolsSessionAndroid(
      content::DevToolsAgentHostClientChannel* channel);

  ChromeDevToolsSessionAndroid(const ChromeDevToolsSessionAndroid&) = delete;
  ChromeDevToolsSessionAndroid& operator=(const ChromeDevToolsSessionAndroid&) =
      delete;

  ~ChromeDevToolsSessionAndroid() override;

  TargetHandlerAndroid* target_handler() { return target_handler_.get(); }

 private:
  std::unique_ptr<AutofillHandler> autofill_handler_;
  std::unique_ptr<BrowserHandlerAndroid> browser_handler_;
  std::unique_ptr<TargetHandlerAndroid> target_handler_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_SESSION_ANDROID_H_
