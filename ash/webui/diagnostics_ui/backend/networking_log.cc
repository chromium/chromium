// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/networking_log.h"

#include <sstream>
#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace ash {
namespace diagnostics {
namespace {

const char kNewline[] = "\n";

// NetworkingInfo constants:
const char kNetworkingInfoSectionName[] = "--- Networking Info ---";

}  // namespace

NetworkingLog::NetworkingLog() = default;
NetworkingLog::~NetworkingLog() = default;

std::string NetworkingLog::GetContents() const {
  std::stringstream output;
  // TODO(michaelcheco): Populate log with network info.
  output << kNetworkingInfoSectionName << kNewline;
  return output.str();
}

}  // namespace diagnostics
}  // namespace ash
