// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOBSTER_LOBSTER_IMAGE_INSERT_OR_COPY_ACTUATOR_H_
#define ASH_LOBSTER_LOBSTER_IMAGE_INSERT_OR_COPY_ACTUATOR_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {

void ASH_EXPORT CopyToClipboard(const std::string& image_bytes);

bool ASH_EXPORT InsertImageOrCopyToClipboard(ui::TextInputClient* input_client,
                                             const std::string& image_bytes);

}  // namespace ash

#endif  // ASH_LOBSTER_LOBSTER_IMAGE_INSERT_OR_COPY_ACTUATOR_H_
