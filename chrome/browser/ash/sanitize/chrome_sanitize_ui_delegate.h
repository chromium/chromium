// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SANITIZE_CHROME_SANITIZE_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_SANITIZE_CHROME_SANITIZE_UI_DELEGATE_H_

#include "ash/webui/sanitize_ui/sanitize_ui_delegate.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace content {
class WebUI;
}  // namespace content

class ProfileResetter;
class PrefService;

namespace ash {

class ChromeSanitizeUIDelegate : public SanitizeUIDelegate {
 public:
  explicit ChromeSanitizeUIDelegate(content::WebUI* web_ui);
  ~ChromeSanitizeUIDelegate() override;
  ChromeSanitizeUIDelegate(const ChromeSanitizeUIDelegate&) = delete;
  ChromeSanitizeUIDelegate& operator=(const ChromeSanitizeUIDelegate&) = delete;

  void PerformSanitizeSettings() override;

  virtual ProfileResetter* GetResetter();

  // Sets |restart_attempt_| for testing. This helps testing as it can block
  // to restart.
  void SetAttemptRestartForTesting(
      const base::RepeatingClosure& restart_attempt) override;

 private:
  // Callback for when the sanitize is done.
  void OnSanitizeDone();

  // Restarts chrome at the end of sanitize. It could be configured for tests.
  base::RepeatingClosure restart_attempt_;

  std::unique_ptr<ProfileResetter> resetter_;
  raw_ptr<PrefService> pref_service_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SANITIZE_CHROME_SANITIZE_UI_DELEGATE_H_
