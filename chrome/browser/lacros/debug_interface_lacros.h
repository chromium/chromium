// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_DEBUG_INTERFACE_LACROS_H_
#define CHROME_BROWSER_LACROS_DEBUG_INTERFACE_LACROS_H_

#include "chromeos/crosapi/mojom/debug_interface.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {

class DebugInterfaceLacros : public mojom::DebugInterface {
 public:
  DebugInterfaceLacros();
  DebugInterfaceLacros(const DebugInterfaceLacros&) = delete;
  DebugInterfaceLacros& operator=(const DebugInterfaceLacros&) = delete;
  ~DebugInterfaceLacros() override;

  // mojom::DebugInterface:
  void PrintUiHierarchy(mojom::PrintTarget target) override;

 private:
  void PrintLayerHierarchy();
  void PrintWindowHierarchy();
  void PrintViewHierarchy();

  mojo::Receiver<mojom::DebugInterface> receiver_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_DEBUG_INTERFACE_LACROS_H_
