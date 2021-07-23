// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_NETWORKING_LOG_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_NETWORKING_LOG_H_

#include <string>

#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"

namespace ash {
namespace diagnostics {

class NetworkingLog {
 public:
  NetworkingLog();

  NetworkingLog(const NetworkingLog&) = delete;
  NetworkingLog& operator=(const NetworkingLog&) = delete;

  ~NetworkingLog();

  // Returns the networking log as a string.
  std::string GetContents() const;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_NETWORKING_LOG_H_
