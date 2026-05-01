// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/default_browser/default_browser_setter.h"

class Profile;

namespace default_browser {

// A setter that opens the Settings Panel UI with a visual guide to assist the
// user in setting Chrome as the default browser on Windows.
class DefaultBrowserVisualGuidedSetter : public DefaultBrowserSetter {
 public:
  explicit DefaultBrowserVisualGuidedSetter(Profile& profile);

  DefaultBrowserVisualGuidedSetter(const DefaultBrowserVisualGuidedSetter&) =
      delete;
  DefaultBrowserVisualGuidedSetter& operator=(
      const DefaultBrowserVisualGuidedSetter&) = delete;

  ~DefaultBrowserVisualGuidedSetter() override;

  // DefaultBrowserSetter:
  DefaultBrowserSetterType GetType() const override;
  void Execute(DefaultBrowserSetterCompletionCallback on_complete) override;

 private:
  const raw_ref<Profile> profile_;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_H_
