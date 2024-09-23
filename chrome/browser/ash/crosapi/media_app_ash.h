// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_MEDIA_APP_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_MEDIA_APP_ASH_H_

#include "chromeos/crosapi/mojom/media_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

class MediaAppAsh : public mojom::MediaApp {
 public:
  MediaAppAsh();
  ~MediaAppAsh() override;
  MediaAppAsh(const MediaAppAsh&) = delete;
  MediaAppAsh& operator=(const MediaAppAsh&) = delete;

  void BindRemote(mojo::PendingRemote<mojom::MediaApp> remote);

  // mojom::MediaApp:
  void SubmitForm(const GURL& url,
                  const std::vector<int8_t>& payload,
                  const std::string& header,
                  mojom::MediaApp::SubmitFormCallback callback) override;

 private:
  mojo::Remote<mojom::MediaApp> media_app_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_MEDIA_APP_ASH_H_
