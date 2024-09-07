// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UITEST_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UITEST_UTIL_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
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
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  GenerationPopup popup_showing_ = GenerationPopup::kHidden;
  GenerationUIState state_ =
      PasswordGenerationPopupController::kOfferGeneration;
};

// An Autofill client that can block until ShowAutofillSuggestions() is called.
class ObservingAutofillClient : public autofill::ChromeAutofillClient {
 public:
  explicit ObservingAutofillClient(content::WebContents* web_contents);

  // Blocks the current thread until ShowAutofillSuggestions() is called.
  void WaitForAutofillPopup();

  SuggestionUiSessionId ShowAutofillSuggestions(
      const autofill::AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<autofill::AutofillSuggestionDelegate> delegate) override;

 private:
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UITEST_UTIL_H_
