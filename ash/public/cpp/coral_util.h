// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CORAL_UTIL_H_
#define ASH_PUBLIC_CPP_CORAL_UTIL_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"

namespace ash::coral_util {

std::string ASH_PUBLIC_EXPORT
GetIdentifier(const coral::mojom::EntityKeyPtr& key);

std::string ASH_PUBLIC_EXPORT GetIdentifier(const coral::mojom::EntityKey& key);

std::string ASH_PUBLIC_EXPORT GetIdentifier(const coral::mojom::Entity& item);

}  // namespace ash::coral_util

#endif  // ASH_PUBLIC_CPP_CORAL_UTIL_H_
