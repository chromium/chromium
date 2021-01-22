// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace platform_keys {

class PlatformKeysService;

namespace test_util {
// A helper that waits until execution of an asynchronous PlatformKeysService
// operation has finished and provides access to the results.
// Note: all PlatformKeysService operations have a trailing status argument that
// is filled in case of an error.
template <typename... ResultCallbackArgs>
class ExecutionWaiter {
 public:
  ExecutionWaiter() = default;
  ~ExecutionWaiter() = default;
  ExecutionWaiter(const ExecutionWaiter& other) = delete;
  ExecutionWaiter& operator=(const ExecutionWaiter& other) = delete;

  // Returns the callback to be passed to the PlatformKeysService operation
  // invocation.
  base::OnceCallback<void(ResultCallbackArgs... result_callback_args,
                          Status status)>
  GetCallback() {
    return base::BindOnce(&ExecutionWaiter::OnExecutionDone,
                          weak_ptr_factory_.GetWeakPtr());
  }

  // Waits until the callback returned by GetCallback() has been called.
  void Wait() { run_loop_.Run(); }

  // Returns the status passed to the callback.
  Status status() const {
    EXPECT_TRUE(done_);
    return status_;
  }

 protected:
  // A std::tuple that is capable of storing the arguments passed to the result
  // callback.
  using ResultCallbackArgsTuple =
      std::tuple<std::decay_t<ResultCallbackArgs>...>;

  // Access to the arguments passed to the callback.
  const ResultCallbackArgsTuple& result_callback_args() const {
    EXPECT_TRUE(done_);
    return result_callback_args_;
  }

 private:
  void OnExecutionDone(ResultCallbackArgs... result_callback_args,
                       Status status) {
    EXPECT_FALSE(done_);
    done_ = true;
    result_callback_args_ = ResultCallbackArgsTuple(
        std::forward<ResultCallbackArgs>(result_callback_args)...);
    status_ = status;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool done_ = false;
  ResultCallbackArgsTuple result_callback_args_;
  Status status_ = Status::kSuccess;

  base::WeakPtrFactory<ExecutionWaiter> weak_ptr_factory_{this};
};

// Supports waiting for the result of PlatformKeysService::GetTokens.
class GetTokensExecutionWaiter
    : public ExecutionWaiter<std::unique_ptr<std::vector<TokenId>>> {
 public:
  GetTokensExecutionWaiter();
  ~GetTokensExecutionWaiter();

  const std::unique_ptr<std::vector<TokenId>>& token_ids() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::GenerateKey*
// function family.
class GenerateKeyExecutionWaiter : public ExecutionWaiter<const std::string&> {
 public:
  GenerateKeyExecutionWaiter();
  ~GenerateKeyExecutionWaiter();

  const std::string& public_key_spki_der() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::Sign* function
// family.
class SignExecutionWaiter : public ExecutionWaiter<const std::string&> {
 public:
  SignExecutionWaiter();
  ~SignExecutionWaiter();

  const std::string& signature() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::GetCertificates.
class GetCertificatesExecutionWaiter
    : public ExecutionWaiter<std::unique_ptr<net::CertificateList>> {
 public:
  GetCertificatesExecutionWaiter();
  ~GetCertificatesExecutionWaiter();

  const net::CertificateList& matches() const {
    return *std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the
// PlatformKeysService::SetAttributeForKey.
class SetAttributeForKeyExecutionWaiter : public ExecutionWaiter<> {
 public:
  SetAttributeForKeyExecutionWaiter();
  ~SetAttributeForKeyExecutionWaiter();
};

// Supports waiting for the result of the
// PlatformKeysService::GetAttributeForKey.
class GetAttributeForKeyExecutionWaiter
    : public ExecutionWaiter<const base::Optional<std::string>&> {
 public:
  GetAttributeForKeyExecutionWaiter();
  ~GetAttributeForKeyExecutionWaiter();

  const base::Optional<std::string>& attribute_value() const {
    return std::get<0>(result_callback_args());
  }
};

// Supports waiting for the result of the PlatformKeysService::RemoveKey.
class RemoveKeyExecutionWaiter : public ExecutionWaiter<> {
 public:
  RemoveKeyExecutionWaiter();
  ~RemoveKeyExecutionWaiter();
};

class GetAllKeysExecutionWaiter
    : public ExecutionWaiter<std::vector<std::string>> {
 public:
  GetAllKeysExecutionWaiter();
  ~GetAllKeysExecutionWaiter();

  const std::vector<std::string>& public_keys() const {
    return std::get<0>(result_callback_args());
  }
};

class IsKeyOnTokenExecutionWaiter
    : public ExecutionWaiter<base::Optional<bool>> {
 public:
  IsKeyOnTokenExecutionWaiter();
  ~IsKeyOnTokenExecutionWaiter();

  base::Optional<bool> on_slot() const {
    return std::get<0>(result_callback_args());
  }
};

class GetKeyLocationsExecutionWaiter
    : public ExecutionWaiter<const std::vector<TokenId>&> {
 public:
  GetKeyLocationsExecutionWaiter();
  ~GetKeyLocationsExecutionWaiter();

  const std::vector<TokenId>& key_locations() const {
    return std::get<0>(result_callback_args());
  }
};

}  // namespace test_util
}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_SERVICE_TEST_UTIL_H_
