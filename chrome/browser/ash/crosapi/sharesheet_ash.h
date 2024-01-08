// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SHARESHEET_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SHARESHEET_ASH_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chromeos/crosapi/mojom/sharesheet.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class Profile;

namespace crosapi {

// Implements the crosapi interface for the Sharesheet service. Lives in
// Ash-Chrome on the UI thread.
class SharesheetAsh : public mojom::Sharesheet, public ProfileObserver {
 public:
  SharesheetAsh();
  SharesheetAsh(const SharesheetAsh&) = delete;
  SharesheetAsh& operator=(const SharesheetAsh&) = delete;
  ~SharesheetAsh() override;

  void MaybeSetProfile(Profile* profile);

  void BindReceiver(mojo::PendingReceiver<mojom::Sharesheet> receiver);

  // crosapi::mojom::Sharesheet:
  void ShowBubble(const std::string& window_id,
                  sharesheet::LaunchSource source,
                  crosapi::mojom::IntentPtr intent,
                  ShowBubbleCallback callback) override;
  void ShowBubbleWithOnClosed(const std::string& window_id,
                              sharesheet::LaunchSource source,
                              crosapi::mojom::IntentPtr intent,
                              ShowBubbleWithOnClosedCallback callback) override;
  void CloseBubble(const std::string& window_id) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  raw_ptr<Profile> profile_ = nullptr;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
  mojo::ReceiverSet<mojom::Sharesheet> receivers_;
  base::WeakPtrFactory<SharesheetAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SHARESHEET_ASH_H_
