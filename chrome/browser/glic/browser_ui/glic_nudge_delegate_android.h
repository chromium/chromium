// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_DELEGATE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"

class BrowserWindowInterface;

namespace ui {
class WindowAndroid;
}

namespace glic {

class GlicNudgeController;
enum class GlicNudgeActivity;

// C++ implementation of GlicNudgeDelegate for Android.
// Acts as JNI bridge to forward C++ nudge trigger/hide calls to Java's
// GlicNudgeDelegateBridge.
class GlicNudgeDelegateAndroid : public GlicSplitButtonDelegate {
 public:
  GlicNudgeDelegateAndroid(GlicNudgeController* controller,
                           BrowserWindowInterface* browser);
  GlicNudgeDelegateAndroid(const GlicNudgeDelegateAndroid&) = delete;
  GlicNudgeDelegateAndroid& operator=(const GlicNudgeDelegateAndroid&) = delete;
  ~GlicNudgeDelegateAndroid() override;

  // GlicSplitButtonDelegate:
  void OnTriggerGlicNudgeUI(NudgeParams params) override;
  void OnHideGlicNudgeUI() override;
  bool GetIsShowingGlicNudge() override;

  void OnNudgeActivity(GlicNudgeActivity activity);

  ui::WindowAndroid* GetWindowAndroid();

 private:
  raw_ptr<GlicNudgeController> controller_ = nullptr;
  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_DELEGATE_ANDROID_H_
