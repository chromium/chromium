// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_FAKE_CWS_MIXIN_H_
#define CHROME_BROWSER_ASH_APP_MODE_FAKE_CWS_MIXIN_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chrome/browser/app_mode/test/fake_origin_test_server_mixin.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "url/gurl.h"

namespace ash {

// Owns and sets up a fake instance of CWS (Chrome Web Store) using `FakeCWS`.
class FakeCwsMixin : InProcessBrowserTestMixin {
 public:
  enum CwsInstanceType {
    // Make `fake_cws_` behave like the regular public CWS store.
    kPublic,
    // Make `fake_cws_` behave like a private server for self hosted extensions.
    kPrivate
  };

  FakeCwsMixin(InProcessBrowserTestMixinHost* host,
               CwsInstanceType instance_type);
  FakeCwsMixin(const FakeCwsMixin&) = delete;
  FakeCwsMixin& operator=(const FakeCwsMixin&) = delete;
  ~FakeCwsMixin() override;

  FakeCWS& fake_cws() { return fake_cws_; }

  // Returns the update URL configured for the `fake_cws_` instance.
  //
  // Note the URL returned is always valid but the underlying server only starts
  // accepting connections in `SetUpOnMainThread`.
  GURL UpdateUrl() const;

  // InProcessBrowserTestMixin overrides:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 private:
  CwsInstanceType instance_type_;

  std::optional<base::AutoReset<bool>> disable_crx_publisher_verification_;

  FakeOriginTestServerMixin fake_origin_mixin_;

  FakeCWS fake_cws_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_FAKE_CWS_MIXIN_H_
