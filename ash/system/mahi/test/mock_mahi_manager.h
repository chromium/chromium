// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_TEST_MOCK_MAHI_MANAGER_H_
#define ASH_SYSTEM_MAHI_TEST_MOCK_MAHI_MANAGER_H_

#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// A mock class for testing.
class MockMahiManager : public chromeos::MahiManager {
 public:
  MockMahiManager();
  MockMahiManager(const MockMahiManager&) = delete;
  MockMahiManager& operator=(const MockMahiManager&) = delete;
  ~MockMahiManager() override;

  // chromeos::MahiManager:
  MOCK_METHOD(void,
              AnswerQuestion,
              (const std::u16string&, bool, MahiAnswerQuestionCallback),
              (override));
  MOCK_METHOD(gfx::ImageSkia, GetContentIcon, (), (override));
  MOCK_METHOD(std::u16string, GetContentTitle, (), (override));
  MOCK_METHOD(GURL, GetContentUrl, (), (override));
  MOCK_METHOD(void, GetOutlines, (MahiOutlinesCallback), (override));
  MOCK_METHOD(void,
              GetSuggestedQuestion,
              (MahiGetSuggestedQuestionCallback),
              (override));
  MOCK_METHOD(void, GetContent, (MahiContentCallback), (override));
  MOCK_METHOD(void, GetSummary, (MahiSummaryCallback), (override));
  MOCK_METHOD(void, GoToOutlineContent, (int), (override));
  MOCK_METHOD(void,
              OnContextMenuClicked,
              (crosapi::mojom::MahiContextMenuRequestPtr),
              (override));
  MOCK_METHOD(void, OpenFeedbackDialog, (), (override));
  MOCK_METHOD(void, OpenMahiPanel, (int64_t, const gfx::Rect&), (override));
  MOCK_METHOD(void,
              SetCurrentFocusedPageInfo,
              (crosapi::mojom::MahiPageInfoPtr),
              (override));
  MOCK_METHOD(bool, IsEnabled, (), (override));
  MOCK_METHOD(void, SetMediaAppPDFFocused, (), (override));
  MOCK_METHOD(std::optional<base::UnguessableToken>,
              GetMediaAppPDFClientId,
              (),
              (const override));
  MOCK_METHOD(bool, AllowRepeatingAnswers, (), (override));
  MOCK_METHOD(void,
              AnswerQuestionRepeating,
              (const std::u16string&,
               bool,
               MahiAnswerQuestionCallbackRepeating),
              (override));
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_TEST_MOCK_MAHI_MANAGER_H_
