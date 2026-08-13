// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_HOST_ANDROID_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_HOST_ANDROID_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host.h"

class Profile;

namespace content {
class WebContents;
}

namespace glic {

class BottomSheetSession;

class GlicExperimentalOptInUIHostAndroid : public GlicExperimentalOptInUIHost {
 public:
  GlicExperimentalOptInUIHostAndroid(Profile* profile, Delegate* delegate);
  ~GlicExperimentalOptInUIHostAndroid() override;

  // GlicExperimentalOptInUIHost:
  void Show(content::WebContents* web_contents) override;
  void Close(bool accepted) override;
  content::WebContents* GetOrCreateSuitableWebContents() override;

  // Android-specific test accessor to simulate the native UI closing.
  void SimulateClosingBottomSheetForTesting();

 private:
  void OnSessionClosed(bool accepted);

  raw_ptr<Profile> profile_;
  raw_ptr<Delegate> delegate_;
  std::unique_ptr<BottomSheetSession> session_;
  base::WeakPtrFactory<GlicExperimentalOptInUIHostAndroid> weak_ptr_factory_{
      this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_UI_HOST_ANDROID_H_
