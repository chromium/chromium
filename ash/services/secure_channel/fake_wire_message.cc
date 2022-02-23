// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/secure_channel/fake_wire_message.h"

#include <memory>
#include <string>

#include "ash/services/secure_channel/wire_message.h"

namespace ash::secure_channel {

FakeWireMessage::FakeWireMessage(const std::string& payload,
                                 const std::string& feature)
    : WireMessage(payload, feature) {}

std::string FakeWireMessage::Serialize() const {
  return feature() + "," + payload();
}

}  // namespace ash::secure_channel
