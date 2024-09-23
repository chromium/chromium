// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_MEDIA_APP_LACROS_H_
#define CHROME_BROWSER_LACROS_MEDIA_APP_LACROS_H_

#include "chromeos/crosapi/mojom/media_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

class MediaAppLacros : public mojom::MediaApp {
 public:
  MediaAppLacros();
  ~MediaAppLacros() override;
  MediaAppLacros(const MediaAppLacros&) = delete;
  MediaAppLacros& operator=(const MediaAppLacros&) = delete;

  // mojom::MediaApp:
  void SubmitForm(const GURL& url,
                  const std::vector<int8_t>& payload,
                  const std::string& header,
                  SubmitFormCallback callback) override;

 private:
  mojo::Receiver<mojom::MediaApp> receiver_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_MEDIA_APP_LACROS_H_
