// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_SEA_PEN_PROVIDER_H_
#define ASH_WEBUI_COMMON_SEA_PEN_PROVIDER_H_

#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::common {

// An interface for binding mojom::SeaPenProvider to a WebUI. Used for
// VC Background and Personalization WebUIs.
class SeaPenProvider {
 public:
  virtual ~SeaPenProvider() = default;

  virtual void BindInterface(
      mojo::PendingReceiver<::ash::personalization_app::mojom::SeaPenProvider>
          receiver) = 0;

  // Determines if the current active profile is eligible to see the SeaPen UI.
  virtual bool IsEligibleForSeaPen() = 0;

  virtual bool IsEligibleForSeaPenTextInput() = 0;
};

}  // namespace ash::common

#endif  // ASH_WEBUI_COMMON_SEA_PEN_PROVIDER_H_
