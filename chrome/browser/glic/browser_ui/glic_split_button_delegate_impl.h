// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_DELEGATE_IMPL_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/browser_ui/gemini_split_button_delegate.h"

class BrowserWindowInterface;

namespace glic {

class GlicKeyedService;

// Concrete implementation of GeminiSplitButtonDelegate for Glic.
class GlicSplitButtonDelegateImpl : public GeminiSplitButtonDelegate {
 public:
  GlicSplitButtonDelegateImpl(BrowserWindowInterface* browser,
                              GlicKeyedService* glic_service);
  GlicSplitButtonDelegateImpl(const GlicSplitButtonDelegateImpl&) = delete;
  GlicSplitButtonDelegateImpl& operator=(const GlicSplitButtonDelegateImpl&) =
      delete;
  ~GlicSplitButtonDelegateImpl() override;

  // GeminiSplitButtonDelegate:
  base::CallbackListSubscription RegisterEnabledChangedCallback(
      base::RepeatingClosure callback) override;
  bool IsEnabled() const override;
  bool IsPanelShowing() const override;
  base::CallbackListSubscription RegisterPanelVisibilityChangedCallback(
      base::RepeatingClosure callback) override;
  void MaybeRecordStartupMetrics() override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<GlicKeyedService> glic_service_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_DELEGATE_IMPL_H_
