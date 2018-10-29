// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_CLIENT_H_
#define ASH_PUBLIC_CPP_ASH_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
namespace ash_client {

// Initializes a client application (e.g. keyboard shortcut viewer) such that
// it can communicate with the ash window manager.
ASH_PUBLIC_EXPORT void Init();

}  // namespace ash_client
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_CLIENT_H_
