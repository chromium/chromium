// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_client.h"

#include "ash/public/cpp/mus_property_mirror_ash.h"
#include "ash/public/cpp/window_properties.h"
#include "ui/views/mus/mus_client.h"

namespace ash {
namespace ash_client {

void Init() {
  // Register ash specific window properties to be transported.
  views::MusClient* mus_client = views::MusClient::Get();
  aura::WindowTreeClientDelegate* delegate = mus_client;
  RegisterWindowProperties(delegate->GetPropertyConverter());

  // Setup property mirror between window and host.
  mus_client->SetMusPropertyMirror(std::make_unique<MusPropertyMirrorAsh>());
}

}  // namespace ash_client
}  // namespace ash
