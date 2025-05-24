// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_CLIENT_IMPL_H_

#include <optional>
#include <string>

#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "ash/public/cpp/lobster/lobster_text_input_context.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/lobster/lobster_service.h"
#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"

namespace ash {
struct LobsterSystemState;
}  // namespace ash

class LobsterClientImpl : public ash::LobsterClient {
 public:
  explicit LobsterClientImpl(LobsterService* service);
  ~LobsterClientImpl() override;

  // LobsterClient overrides
  void SetActiveSession(ash::LobsterSession* session) override;
  ash::LobsterSystemState GetSystemState(
      const ash::LobsterTextInputContext& text_input_context) override;
  void RequestCandidates(const std::string& query,
                         int num_candidates,
                         ash::RequestCandidatesCallback) override;
  void InflateCandidate(uint32_t seed,
                        const std::string& query,
                        ash::InflateCandidateCallback callback) override;
  void QueueInsertion(const std::string& image_bytes,
                      StatusCallback insert_status_callback) override;
  void ShowDisclaimerUI() override;
  void LoadUI(std::optional<std::string> query,
              ash::LobsterMode mode,
              const gfx::Rect& caret_bounds) override;
  void ShowUI() override;
  void CloseUI() override;
  const AccountId& GetAccountId() override;
  void AnnounceLater(const std::u16string& message) override;

 private:
  // Not owned by this class
  raw_ptr<LobsterService> service_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_CLIENT_IMPL_H_
