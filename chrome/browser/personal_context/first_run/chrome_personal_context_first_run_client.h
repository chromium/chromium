// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSONAL_CONTEXT_FIRST_RUN_CHROME_PERSONAL_CONTEXT_FIRST_RUN_CLIENT_H_
#define CHROME_BROWSER_PERSONAL_CONTEXT_FIRST_RUN_CHROME_PERSONAL_CONTEXT_FIRST_RUN_CLIENT_H_

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/personal_context/first_run/personal_context_first_run_client.h"

namespace content {
class WebContents;
}

#if BUILDFLAG(IS_ANDROID)
namespace personal_context {
class PersonalContextFirstRunBottomSheetBridge;
}
#endif

class ChromePersonalContextFirstRunClient
    : public personal_context::PersonalContextFirstRunClient {
 public:
  ChromePersonalContextFirstRunClient();
  ChromePersonalContextFirstRunClient(
      const ChromePersonalContextFirstRunClient&) = delete;
  ChromePersonalContextFirstRunClient& operator=(
      const ChromePersonalContextFirstRunClient&) = delete;
  ~ChromePersonalContextFirstRunClient() override;

  // PersonalContextFirstRunClient:
  void ShowNotice(content::WebContents* web_contents,
                  personal_context::FirstRunInvocationSource invocation_source,
                  base::OnceCallback<void(personal_context::NoticeResult)>
                      callback) override;

 private:
#if BUILDFLAG(IS_ANDROID)
  void OnNoticeResult(
      base::OnceCallback<void(personal_context::NoticeResult)> callback,
      personal_context::NoticeResult result);

  std::unique_ptr<personal_context::PersonalContextFirstRunBottomSheetBridge>
      android_bridge_;
#endif
};

#endif  // CHROME_BROWSER_PERSONAL_CONTEXT_FIRST_RUN_CHROME_PERSONAL_CONTEXT_FIRST_RUN_CLIENT_H_
