// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOBSTER_LOBSTER_IMAGE_ACTUATOR_H_
#define ASH_LOBSTER_LOBSTER_IMAGE_ACTUATOR_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/text_input_client.h"
#include "url/gurl.h"

namespace ash {

class ASH_EXPORT LobsterImageActuator {
 public:
  LobsterImageActuator();
  ~LobsterImageActuator();

  void InsertImageOrCopyToClipboard(ui::TextInputClient* input_client,
                                    const std::string& image_bytes);

 private:
};

}  // namespace ash

#endif  // ASH_LOBSTER_LOBSTER_IMAGE_ACTUATOR_H_
