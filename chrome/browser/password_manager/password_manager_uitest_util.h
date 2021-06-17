// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UITEST_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UITEST_UTIL_H_

#include "base/run_loop.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/passwords/password_generation_popup_observer.h"

using GenerationUIState = PasswordGenerationPopupController::GenerationUIState;

class TestGenerationPopupObserver : public PasswordGenerationPopupObserver {
 public:
  enum class GenerationPopup {
    kShown,
    kHidden,
  };

  TestGenerationPopupObserver() = default;
  TestGenerationPopupObserver(const TestGenerationPopupObserver&) = delete;
  TestGenerationPopupObserver& operator=(const TestGenerationPopupObserver&) =
      delete;
  virtual ~TestGenerationPopupObserver() = default;

  // PasswordGenerationPopupObserver
  void OnPopupShown(GenerationUIState state) override;
  void OnPopupHidden() override;

  bool popup_showing() const;
  GenerationUIState state() const;

  // Waits until the popup is in specified status.
  void WaitForStatus(GenerationPopup status);

  // Waits until the popup is either shown or hidden.
  void WaitForStatusChange();

 private:
  void MaybeQuitRunLoop();

  // The loop to be stopped after the popup state change.
  base::RunLoop* run_loop_ = nullptr;
  GenerationPopup popup_showing_ = GenerationPopup::kHidden;
  GenerationUIState state_ =
      PasswordGenerationPopupController::kOfferGeneration;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UITEST_UTIL_H_
