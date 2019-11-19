// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_server.h"

#include <string>

namespace chromeos {

PrintServer::PrintServer(const std::string& id,
                         const GURL& url,
                         const std::string& name)
    : id_(id), url_(url), name_(name) {}

}  // namespace chromeos
