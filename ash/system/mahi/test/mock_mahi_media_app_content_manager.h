// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_TEST_MOCK_MAHI_MEDIA_APP_CONTENT_MANAGER_H_
#define ASH_SYSTEM_MAHI_TEST_MOCK_MAHI_MEDIA_APP_CONTENT_MANAGER_H_

#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
class MockMahiMediaAppContentManager
    : public chromeos::MahiMediaAppContentManager {
 public:
  MockMahiMediaAppContentManager();
  MockMahiMediaAppContentManager(const MockMahiMediaAppContentManager&) =
      delete;
  MockMahiMediaAppContentManager& operator=(
      const MockMahiMediaAppContentManager&) = delete;
  ~MockMahiMediaAppContentManager() override;

  // chromeos::MahiMediaAppContentManager:
  MOCK_METHOD(std::optional<std::string>,
              GetFileName,
              (const base::UnguessableToken),
              (override));
  MOCK_METHOD(void,
              GetContent,
              (const base::UnguessableToken,
               chromeos::GetMediaAppContentCallback),
              (override));
  MOCK_METHOD(void,
              OnMahiContextMenuClicked,
              (int64_t,
               chromeos::mahi::ButtonType,
               const std::u16string&,
               const gfx::Rect&),
              (override));
  MOCK_METHOD(void,
              AddClient,
              (base::UnguessableToken, ash::MahiMediaAppClient*),
              (override));
  MOCK_METHOD(void, RemoveClient, (base::UnguessableToken), (override));
  MOCK_METHOD(bool, ObservingWindow, (const aura::Window*), (const override));
  MOCK_METHOD(bool,
              ActivateClientWindow,
              (const base::UnguessableToken),
              (override));
};
}  // namespace mahi

#endif  // ASH_SYSTEM_MAHI_TEST_MOCK_MAHI_MEDIA_APP_CONTENT_MANAGER_H_
