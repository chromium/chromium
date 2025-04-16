// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_E2E_TESTS_LIVE_TEST_H_
#define CHROME_BROWSER_SIGNIN_E2E_TESTS_LIVE_TEST_H_

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/signin/public/identity_manager/test_accounts.h"

namespace signin::test {

// Discovers if the environment allows live testing. Otherwise, it will skip the
// test.
class LiveTestMixin : public InProcessBrowserTestMixin {
 public:
  explicit LiveTestMixin(InProcessBrowserTestMixinHost* host);
  LiveTestMixin(const LiveTestMixin&) = delete;
  LiveTestMixin& operator=(const LiveTestMixin&) = delete;
  ~LiveTestMixin() override = default;

  // InProcessBrowserTestMixin
  void SetUp() override;

  bool Enabled() const { return enabled_; }

 private:
  bool enabled_ = false;
};

// Reads test accounts from a static file
class TestAccountsMixin : public InProcessBrowserTestMixin {
 public:
  explicit TestAccountsMixin(InProcessBrowserTestMixinHost* host);
  TestAccountsMixin(const TestAccountsMixin&) = delete;
  TestAccountsMixin& operator=(const TestAccountsMixin&) = delete;
  ~TestAccountsMixin() override;

  // InProcessBrowserTestMixin
  void SetUp() override;

  const TestAccountsConfig& GetTestAccounts() const { return *test_accounts_; }
  bool AreAccountsLoaded() const { return test_accounts_.has_value(); }

 private:
  std::optional<TestAccountsConfig> test_accounts_;
};

// Allows lookup of Google-specific signin related hosts.
class DirectLookupMixin : public InProcessBrowserTestMixin {
 public:
  DirectLookupMixin(InProcessBrowserTestMixinHost* host,
                    InProcessBrowserTest* test_base);
  DirectLookupMixin(const DirectLookupMixin&) = delete;
  DirectLookupMixin& operator=(const DirectLookupMixin&) = delete;
  ~DirectLookupMixin() override = default;

  // InProcessBrowserTestMixin
  void SetUpInProcessBrowserTestFixture() override;

 private:
  raw_ptr<InProcessBrowserTest> test_base_;
};

// Test base that offers test accounts, connectivity to google auth endpoints
// and is conditionally enabled with a `run-live-tests` command line switch.
class LiveTest : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUp() override;
  void TearDown() override;
  void PostRunTestOnMainThread() override;

  const TestAccountsConfig* GetTestAccounts() const {
    return &test_accounts_mixin_.GetTestAccounts();
  }

 private:
  LiveTestMixin live_test_mixin_{&mixin_host_};
  DirectLookupMixin direct_lookup_mixin_{&mixin_host_, this};
  TestAccountsMixin test_accounts_mixin_{&mixin_host_};
};

}  // namespace signin::test

#endif  // CHROME_BROWSER_SIGNIN_E2E_TESTS_LIVE_TEST_H_
