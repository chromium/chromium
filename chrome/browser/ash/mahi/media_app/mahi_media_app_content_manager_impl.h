// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_CONTENT_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_CONTENT_MANAGER_IMPL_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"

namespace ash {

// Implementation of MahiMediaAppContentManager to deal with the content request
// to media app PDF files.
// Also behaves as an observer of MahiMediaAppEventsProxy to keep track of the
// active media app instance.
class MahiMediaAppContentManagerImpl
    : public chromeos::MahiMediaAppEventsProxy::Observer,
      public chromeos::MahiMediaAppContentManager {
 public:
  MahiMediaAppContentManagerImpl();
  MahiMediaAppContentManagerImpl(const MahiMediaAppContentManagerImpl&) =
      delete;
  MahiMediaAppContentManagerImpl& operator=(
      const MahiMediaAppContentManagerImpl&) = delete;
  ~MahiMediaAppContentManagerImpl() override;

  // chromeos::MahiMediaAppEventsProxy::Observer
  void OnPdfGetFocus(const base::UnguessableToken client_id) override;

  // chromeos::MahiMediaAppContentManager::
  std::u16string GetFileName(const base::UnguessableToken client_id) override;
  void GetContent(base::UnguessableToken client_id,
                  chromeos::GetMediaAppContentCallback callback) override;
  void OnMahiContextMenuClicked(int64_t display_id,
                                chromeos::mahi::ButtonType button_type,
                                const std::u16string& question) override;

 private:
  // TODO(b/335741382): just stub, replace with real Media App client.
  std::map<base::UnguessableToken, std::string> client_id_to_client_;
  base::UnguessableToken active_client_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MEDIA_APP_MAHI_MEDIA_APP_CONTENT_MANAGER_IMPL_H_
