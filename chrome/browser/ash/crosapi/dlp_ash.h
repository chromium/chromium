// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DLP_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DLP_ASH_H_

#include "base/files/file_path.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implements the crosapi DLP (Data Leak Prevention) interface. Lives in
// ash-chrome on the UI thread.
class DlpAsh : public mojom::Dlp {
 public:
  DlpAsh();
  DlpAsh(const DlpAsh&) = delete;
  DlpAsh& operator=(const DlpAsh&) = delete;
  ~DlpAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Dlp> receiver);

  // crosapi::mojom::Dlp:
  void DlpRestrictionsUpdated(
      const std::string& window_id,
      mojom::DlpRestrictionSetPtr restrictions) override;
  void CheckScreenShareRestriction(
      mojom::ScreenShareAreaPtr area,
      const std::u16string& application_title,
      CheckScreenShareRestrictionCallback callback) override;
  void OnScreenShareStarted(
      const std::string& label,
      mojom::ScreenShareAreaPtr area,
      const ::std::u16string& application_title,
      ::mojo::PendingRemote<mojom::StateChangeDelegate> delegate) override;
  void OnScreenShareStopped(const std::string& label,
                            mojom::ScreenShareAreaPtr area) override;
  void ShowBlockedFiles(std::optional<uint64_t> task_id,
                        const std::vector<base::FilePath>& blocked_files,
                        mojom::FileAction action) override;

 private:
  // Callback to pass request to change screen share state to remote.
  void ChangeScreenShareState(mojo::RemoteSetElementId id,
                              const content::DesktopMediaID& media_id,
                              blink::mojom::MediaStreamStateChange new_state);

  // Callback to pass request to stop screen share to remote.
  void StopScreenShare(mojo::RemoteSetElementId id);

  void OnDisconnect();

  mojo::ReceiverSet<mojom::Dlp> receivers_;
  mojo::RemoteSet<mojom::StateChangeDelegate> screen_share_remote_delegates_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DlpAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DLP_ASH_H_
