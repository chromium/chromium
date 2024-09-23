// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_MOJO_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_MOJO_CONNECTION_MANAGER_H_

#include <memory>
#include <utility>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace crosapi {

// An extension of BrowserManager to help set up and manage the mojo connections
// between the test executable and ash-chrome in testing environment.
//
// In testing environment, the workflow is as following:
// - Ash-chrome creates a Unix domain socket.
// - Test executable connects to the Unix domain socket.
// - When ash-chrome accepts the connection, it creates a |PlatformChannel| and
//   sends one end of it (as a FD) over the socket.
// - Test executable reads the FD from the socket and passes it to lacros-chrome
//   when launching a test.
//
// The workflow works for debugging as well, a wapper script can play the role
// of the test executable above to obtain the FD, and passes it to lacros-chrome
// when launching it inside gdb.
class TestMojoConnectionManager {
 public:
  explicit TestMojoConnectionManager(const base::FilePath& socket_path);

  TestMojoConnectionManager(const TestMojoConnectionManager&) = delete;
  TestMojoConnectionManager& operator=(const TestMojoConnectionManager&) =
      delete;

  ~TestMojoConnectionManager();

 private:
  // Called when the testing socket is created.
  void OnTestingSocketCreated(base::ScopedFD socket_fd);

  // Called when a client, such as a test launcher, attempts to connect.
  void OnTestingSocketAvailable();

  // A socket for a client, such as a test launcher, to connect to.
  base::ScopedFD testing_socket_;

  // A watcher that watches |testing_scoket_| and invokes
  // |OnTestingSocketAvailable| when it becomes readable.
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      testing_socket_watcher_;

  base::WeakPtrFactory<TestMojoConnectionManager> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_MOJO_CONNECTION_MANAGER_H_
