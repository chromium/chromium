// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GEMINI_SPLIT_BUTTON_DELEGATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GEMINI_SPLIT_BUTTON_DELEGATE_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"

namespace glic {

// Interface to abstract feature-specific business logic for the split button
// across different entry points (e.g., Glic vs. GEiC).
// The delegate is owned by GlicSplitButtonController, so its lifetime is
// strictly bounded by the controller's lifetime.
class GeminiSplitButtonDelegate {
 public:
  GeminiSplitButtonDelegate() = default;
  virtual ~GeminiSplitButtonDelegate();

  // Registers a callback that fires whenever the feature enablement status
  // changes (e.g., via pref or enterprise policy update).
  virtual base::CallbackListSubscription RegisterEnabledChangedCallback(
      base::RepeatingClosure callback) = 0;

  // Returns true if the corresponding entry point feature (e.g., Glic or GEiC)
  // is enabled and allowed for the current profile. For non-Glic entry points,
  // this evaluates whether that specific feature is enabled.
  virtual bool IsEnabled() const = 0;

  // Returns true if the associated feature side panel or UI surface is
  // currently open for the browser window.
  virtual bool IsPanelShowing() const = 0;

  // Registers a callback that fires whenever the associated panel's visibility
  // toggles (shown or hidden).
  virtual base::CallbackListSubscription RegisterPanelVisibilityChangedCallback(
      base::RepeatingClosure callback) = 0;

  // Invoked to log feature-specific startup metrics (guaranteed to be recorded
  // at most once per session).
  virtual void MaybeRecordStartupMetrics() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GEMINI_SPLIT_BUTTON_DELEGATE_H_
