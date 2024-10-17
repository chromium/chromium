// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_FAKE_CWS_MIXIN_H_
#define CHROME_BROWSER_ASH_APP_MODE_FAKE_CWS_MIXIN_H_

#include "base/command_line.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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
  GURL UpdateUrl() const;

  // InProcessBrowserTestMixin overrides:
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  CwsInstanceType instance_type_;

  net::EmbeddedTestServer test_server_;
  EmbeddedTestServerSetupMixin test_server_setup_mixin_;

  FakeCWS fake_cws_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_FAKE_CWS_MIXIN_H_
