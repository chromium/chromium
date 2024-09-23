// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_FULL_RESTORE_CLIENT_LACROS_H_
#define CHROME_BROWSER_LACROS_FULL_RESTORE_CLIENT_LACROS_H_

#include "chromeos/crosapi/mojom/full_restore.mojom.h"
#include "components/sessions/core/session_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// This class gathers full restore data for Ash.
class FullRestoreClientLacros : public crosapi::mojom::FullRestoreClient {
 public:
  FullRestoreClientLacros();
  FullRestoreClientLacros(const FullRestoreClientLacros&) = delete;
  FullRestoreClientLacros& operator=(const FullRestoreClientLacros&) = delete;
  ~FullRestoreClientLacros() override;

  // crosapi::mojom::FullRestoreClient:
  void GetSessionInformation(GetSessionInformationCallback callback) override;

 private:
  using SessionWindows = std::vector<std::unique_ptr<sessions::SessionWindow>>;
  using SessionWindowsPair =
      std::pair<SessionWindows, /*lacros_profile_id=*/uint64_t>;

  void OnGotSession(base::OnceCallback<void(SessionWindowsPair)> barrier,
                    uint64_t profile_id,
                    SessionWindows session_windows,
                    SessionID active_window_id,
                    bool read_error);
  void OnGotAllSessions(
      GetSessionInformationCallback callback,
      const std::vector<SessionWindowsPair>& all_session_windows);

  mojo::Receiver<crosapi::mojom::FullRestoreClient> receiver_{this};

  base::WeakPtrFactory<FullRestoreClientLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_FULL_RESTORE_CLIENT_LACROS_H_
