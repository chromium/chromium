// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_FAKE_SHELL_DELEGATE_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_FAKE_SHELL_DELEGATE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "build/buildflag.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/shell_integration.h"
#include "url/gurl.h"

namespace default_browser {

// The production shell delegate rely on Os API. This class is a fake
// implementation for testing purpose allowing the implementer to define the
// expected behavior.
class FakeShellDelegate : public DefaultBrowserManager::ShellDelegate {
 public:
  FakeShellDelegate();
  ~FakeShellDelegate() override;

  void StartCheckIsDefault(
      shell_integration::DefaultWebClientWorkerCallback callback) override;

#if BUILDFLAG(IS_WIN)
  void StartCheckDefaultClientProgId(
      const GURL& scheme,
      base::OnceCallback<void(const std::u16string&)> callback) override;
#endif  // BUILDFLAG(IS_WIN)

  void set_default_state(DefaultBrowserState state) { default_state_ = state; }
  void set_http_assoc_prog_id(const std::u16string& prog_id) {
    http_assoc_prog_id_ = prog_id;
  }

 private:
  DefaultBrowserState default_state_ = shell_integration::NUM_DEFAULT_STATES;
  std::u16string http_assoc_prog_id_ = u"";
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_TEST_SUPPORT_FAKE_SHELL_DELEGATE_H_
