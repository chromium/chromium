// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/async_log.h"

namespace ash {
namespace diagnostics {

AsyncLog::AsyncLog(const base::FilePath& file_path) : file_path_(file_path) {}

AsyncLog::~AsyncLog() = default;

}  // namespace diagnostics
}  // namespace ash
