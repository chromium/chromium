// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_INPUT_ENGINE_H_
#define ASH_SERVICES_IME_INPUT_ENGINE_H_

namespace ash {
namespace ime {

// Base class for all input engines.
struct InputEngine {
  virtual ~InputEngine() = default;

  // Whether the input engine is still connected to the client.
  virtual bool IsConnected() = 0;
};

}  // namespace ime
}  // namespace ash

#endif  // ASH_SERVICES_IME_INPUT_ENGINE_H_
