// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_MOCK_DLP_CROSAPI_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_MOCK_DLP_CROSAPI_H_

#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

// TODO(crbug.com/40818864): Mock Lacros side of crosapi instead when possible.
// This will allow these tests to be just unit_tests, not
// lacros_chrome_browsertests.
class MockDlpCrosapi : public crosapi::mojom::Dlp {
 public:
  MockDlpCrosapi();
  ~MockDlpCrosapi() override;

  MOCK_METHOD(void,
              DlpRestrictionsUpdated,
              (const std::string&, crosapi::mojom::DlpRestrictionSetPtr),
              (override));
  MOCK_METHOD(void,
              CheckScreenShareRestriction,
              (crosapi::mojom::ScreenShareAreaPtr,
               const std::u16string&,
               CheckScreenShareRestrictionCallback),
              (override));
  MOCK_METHOD(void,
              OnScreenShareStarted,
              (const std::string&,
               crosapi::mojom::ScreenShareAreaPtr,
               const ::std::u16string&,
               ::mojo::PendingRemote<crosapi::mojom::StateChangeDelegate>),
              (override));
  MOCK_METHOD(void,
              OnScreenShareStopped,
              (const std::string&, crosapi::mojom::ScreenShareAreaPtr),
              (override));
  MOCK_METHOD(void,
              ShowBlockedFiles,
              (std::optional<uint64_t> task_id,
               const std::vector<base::FilePath>& blocked_files,
               crosapi::mojom::FileAction action),
              (override));
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_MOCK_DLP_CROSAPI_H_
