// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_MOCK_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_MOCK_PASSWORDS_PRIVATE_EVENT_ROUTER_H_

#include <string>
#include <vector>

#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class MockPasswordsPrivateEventRouter : public PasswordsPrivateEventRouter {
 public:
  MockPasswordsPrivateEventRouter();

  MockPasswordsPrivateEventRouter(const MockPasswordsPrivateEventRouter&) =
      delete;
  MockPasswordsPrivateEventRouter& operator=(
      const MockPasswordsPrivateEventRouter&) = delete;

  ~MockPasswordsPrivateEventRouter() override;

  MOCK_METHOD(
      void,
      OnSavedPasswordsListChanged,
      (const std::vector<api::passwords_private::PasswordUiEntry>& entries),
      (override));
  MOCK_METHOD(
      void,
      OnPasswordExceptionsListChanged,
      (const std::vector<api::passwords_private::ExceptionEntry>& exceptions),
      (override));
  MOCK_METHOD(void,
              OnPasswordsExportProgress,
              (api::passwords_private::ExportProgressStatus status,
               const std::string& file_path,
               const std::string& folder_name),
              (override));
  MOCK_METHOD(void,
              OnAccountStorageActiveStateChanged,
              (bool active),
              (override));
  MOCK_METHOD(void,
              OnInsecureCredentialsChanged,
              (std::vector<api::passwords_private::PasswordUiEntry>
                   insecure_credentials),
              (override));
  MOCK_METHOD(void,
              OnPasswordCheckStatusChanged,
              (const api::passwords_private::PasswordCheckStatus& status),
              (override));
  MOCK_METHOD(void, OnPasswordManagerAuthTimeout, (), (override));
  MOCK_METHOD(void,
              OnPasswordManagerActionableErrorChanged,
              (api::passwords_private::PasswordManagerActionableError error),
              (override));
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_MOCK_PASSWORDS_PRIVATE_EVENT_ROUTER_H_
