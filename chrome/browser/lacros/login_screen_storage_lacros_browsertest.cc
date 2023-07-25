// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kKey1[] = "prefix_aaaaa";
constexpr char kKey2[] = "prefix_bbbbb";
constexpr char kKey3[] = "prefix_ccccc";

// For Lacros browsertests, ash chrome initializes an instance of
// FakeSessionManagerClient which returns success (an empty error message) for
// `LoginScreenStorageStore` and a fixed return value for
// `LoginScreenStorageRetrieve`.
// Starting from M117 FakeSessionManagerClient stores the values in memory
// values `LoginScreenStorageStore` and returns them via
// `LoginScreenStorageRetrieve`.
constexpr char kLoginScreenStorageStoreData[] = "data";
constexpr char kLoginScreenStorageRetrieveResult[] = "Test";

bool IsLoginScreenStorageCrosapiAvailable() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::LoginScreenStorage>()) {
    LOG(WARNING) << "Unsupported ash version.";
    return false;
  }
  return true;
}

}  // namespace

using LoginScreenStorageLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(LoginScreenStorageLacrosBrowserTest, Store) {
  if (!IsLoginScreenStorageCrosapiAvailable()) {
    return;
  }

  auto* lacros_service = chromeos::LacrosService::Get();

  crosapi::mojom::LoginScreenStorageMetadataPtr metadata =
      crosapi::mojom::LoginScreenStorageMetadata::New();
  metadata->clear_on_session_exit = true;

  absl::optional<std::string> error_message;
  crosapi::mojom::LoginScreenStorageAsyncWaiter async_waiter(
      lacros_service->GetRemote<crosapi::mojom::LoginScreenStorage>().get());
  async_waiter.Store({kKey1, kKey2}, std::move(metadata),
                     kLoginScreenStorageStoreData, &error_message);

  EXPECT_FALSE(error_message.has_value());
}

IN_PROC_BROWSER_TEST_F(LoginScreenStorageLacrosBrowserTest, Retrieve) {
  if (!IsLoginScreenStorageCrosapiAvailable()) {
    return;
  }

  auto* lacros_service = chromeos::LacrosService::Get();

  crosapi::mojom::LoginScreenStorageRetrieveResultPtr result;
  crosapi::mojom::LoginScreenStorageAsyncWaiter async_waiter(
      lacros_service->GetRemote<crosapi::mojom::LoginScreenStorage>().get());
  // key here is different from the keys in the `Store` test. It's important
  // to not overlap with the `Store` keys because from M117
  // FakeSessionManagerClient implements the API and stores values into the
  // memory. `Store` and `Retrieve` (not `StoreRetrieve`) tests could probably
  // be deleted when M117 hits stable.
  async_waiter.Retrieve(kKey3, &result);

  ASSERT_FALSE(result->is_error_message());
  EXPECT_EQ(result->get_data(), kLoginScreenStorageRetrieveResult);
}

IN_PROC_BROWSER_TEST_F(LoginScreenStorageLacrosBrowserTest, StoreRetrieve) {
  if (!IsLoginScreenStorageCrosapiAvailable()) {
    return;
  }

  auto* lacros_service = chromeos::LacrosService::Get();

  crosapi::mojom::LoginScreenStorageMetadataPtr metadata =
      crosapi::mojom::LoginScreenStorageMetadata::New();
  metadata->clear_on_session_exit = true;

  absl::optional<std::string> error_message;
  crosapi::mojom::LoginScreenStorageAsyncWaiter async_waiter(
      lacros_service->GetRemote<crosapi::mojom::LoginScreenStorage>().get());
  async_waiter.Store({kKey1, kKey2}, std::move(metadata),
                     kLoginScreenStorageStoreData, &error_message);

  EXPECT_FALSE(error_message.has_value());

  crosapi::mojom::LoginScreenStorageRetrieveResultPtr result;

  // Check both keys.
  async_waiter.Retrieve(kKey1, &result);
  ASSERT_FALSE(result->is_error_message());
  EXPECT_EQ(result->get_data(), kLoginScreenStorageStoreData);

  async_waiter.Retrieve(kKey2, &result);
  ASSERT_FALSE(result->is_error_message());
  EXPECT_EQ(result->get_data(), kLoginScreenStorageStoreData);
}
