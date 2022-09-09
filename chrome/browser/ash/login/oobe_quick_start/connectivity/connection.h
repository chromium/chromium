// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_

namespace ash::quick_start {

// Represents a connection to the remote source device and is an abstraction of
// a Nearby Connection.
class Connection {
 public:
  Connection() = default;
  virtual ~Connection() = default;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_CONNECTION_H_
