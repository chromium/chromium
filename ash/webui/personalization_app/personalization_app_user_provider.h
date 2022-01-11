// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_USER_PROVIDER_H_
#define ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_USER_PROVIDER_H_

#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

class PersonalizationAppUserProvider
    : public personalization_app::mojom::UserProvider {
 public:
  virtual void BindInterface(
      mojo::PendingReceiver<personalization_app::mojom::UserProvider>
          receiver) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_PERSONALIZATION_APP_PERSONALIZATION_APP_USER_PROVIDER_H_
