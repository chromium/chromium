// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/print_server.h"

#include <string>

namespace ash {

PrintServer::PrintServer(const std::string& id,
                         const GURL& url,
                         const std::string& name)
    : id_(id), url_(url), name_(name) {}

}  // namespace ash
