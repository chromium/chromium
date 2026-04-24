// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_H_

#include "chrome/browser/default_browser/default_browser_setter.h"

namespace default_browser {

// A setter that opens the Settings Panel UI with a visual guide to assist the
// user in setting Chrome as the default browser on Windows.
class DefaultBrowserVisualGuidedSetter : public DefaultBrowserSetter {
 public:
  DefaultBrowserVisualGuidedSetter();

  DefaultBrowserVisualGuidedSetter(const DefaultBrowserVisualGuidedSetter&) =
      delete;
  DefaultBrowserVisualGuidedSetter& operator=(
      const DefaultBrowserVisualGuidedSetter&) = delete;

  ~DefaultBrowserVisualGuidedSetter() override;

  // DefaultBrowserSetter:
  DefaultBrowserSetterType GetType() const override;
  void Execute(DefaultBrowserSetterCompletionCallback on_complete) override;
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_H_
