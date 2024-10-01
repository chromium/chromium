// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOBSTER_LOBSTER_SESSION_IMPL_H_
#define ASH_LOBSTER_LOBSTER_SESSION_IMPL_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/lobster/lobster_candidate_store.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_feedback_preview.h"
#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace ash {

class LobsterClient;

class ASH_EXPORT LobsterSessionImpl : public LobsterSession {
 public:
  using ActionCallback = base::OnceCallback<void(const std::string&)>;

  explicit LobsterSessionImpl(std::unique_ptr<LobsterClient> client);
  LobsterSessionImpl(std::unique_ptr<LobsterClient> client,
                     const LobsterCandidateStore& candidate_store);

  ~LobsterSessionImpl() override;

  // LobsterSession overrides
  void DownloadCandidate(int candidate_id,
                         const base::FilePath& file_path,
                         StatusCallback callback) override;
  void CommitAsInsert(int candidate_id, StatusCallback callback) override;
  void CommitAsDownload(int candidate_id,
                        const base::FilePath& file_path,
                        StatusCallback callback) override;
  void RequestCandidates(const std::string& query,
                         int num_candidates,
                         RequestCandidatesCallback) override;
  void PreviewFeedback(int candidate_id,
                       LobsterPreviewFeedbackCallback) override;
  bool SubmitFeedback(int candidate_id,
                      const std::string& description) override;
  void LoadUI(std::optional<std::string> query) override;
  void ShowUI() override;
  void CloseUI() override;

 private:
  void OnRequestCandidates(RequestCandidatesCallback callback,
                           const LobsterResult& image_candidates);

  void InflateCandidateAndPerformAction(int candidate_id,
                                        ActionCallback action_callback,
                                        StatusCallback status_callback);

  std::unique_ptr<LobsterClient> client_;

  LobsterCandidateStore candidate_store_;

  base::WeakPtrFactory<LobsterSessionImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOBSTER_LOBSTER_SESSION_IMPL_H_
