// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

namespace glic {

namespace {

class GlicExperimentalOptInUIHostAndroid : public GlicExperimentalOptInUIHost {
 public:
  GlicExperimentalOptInUIHostAndroid(Profile* profile, Delegate* delegate)
      : profile_(profile), delegate_(delegate) {}

  ~GlicExperimentalOptInUIHostAndroid() override = default;

  void Show(content::WebContents* web_contents) override {
    // TODO(crbug.com/484037810): Android UI presentation implementation will be
    // added in a follow-up CL.
    if (delegate_) {
      delegate_->OnUIClosed(/*accepted=*/false);
    }
  }

  void Close(bool accepted) override {}

  content::WebContents* GetOrCreateSuitableWebContents() override {
    // TODO(crbug.com/484037810): Implement Android suitable web contents
    // retrieval.
    NOTREACHED();
  }

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<Delegate> delegate_;
};

}  // namespace

// static
std::unique_ptr<GlicExperimentalOptInUIHost>
GlicExperimentalOptInUIHost::Create(Profile* profile, Delegate* delegate) {
  return std::make_unique<GlicExperimentalOptInUIHostAndroid>(profile,
                                                              delegate);
}

}  // namespace glic
