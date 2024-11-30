// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DESK_TEMPLATE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DESK_TEMPLATE_ASH_H_

#include "chromeos/crosapi/mojom/desk_template.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "url/gurl.h"

namespace crosapi {

// Implements the crosapi interface for desk template. Lives in Ash-Chrome
// on the UI thread.
class DeskTemplateAsh : public mojom::DeskTemplate {
 public:
  DeskTemplateAsh();
  DeskTemplateAsh(const DeskTemplateAsh&) = delete;
  DeskTemplateAsh& operator=(const DeskTemplateAsh&) = delete;
  ~DeskTemplateAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::DeskTemplate> receiver);

  void GetFaviconImage(
      const GURL& url,
      uint64_t lacros_profile_id,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback);

  // crosapi::mojom::DeskTemplate:
  void AddDeskTemplateClient(
      mojo::PendingRemote<mojom::DeskTemplateClient> client) override;

 private:
  mojo::ReceiverSet<mojom::DeskTemplate> receivers_;
  // Each separate Lacros process owns its own remote.
  mojo::RemoteSet<mojom::DeskTemplateClient> remotes_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DESK_TEMPLATE_ASH_H_
