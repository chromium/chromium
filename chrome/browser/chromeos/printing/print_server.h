// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_SERVER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_SERVER_H_

#include <string>

#include "url/gurl.h"

namespace chromeos {

// Simple class representing Print Server.
class PrintServer {
 public:
  PrintServer(const std::string& id, const GURL& url, const std::string& name);

  // Returns server's id.
  const std::string& GetId() const { return id_; }

  // Returns server's URL, used for communication over IPP protocol.
  const GURL& GetUrl() const { return url_; }

  // Returns server's name for end-users.
  const std::string& GetName() const { return name_; }

  // Comparison operator.
  bool operator==(const PrintServer& obj) const {
    return url_ == obj.url_ && id_ == obj.id_ && name_ == obj.name_;
  }

 private:
  std::string id_;
  GURL url_;
  std::string name_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_SERVER_H_
