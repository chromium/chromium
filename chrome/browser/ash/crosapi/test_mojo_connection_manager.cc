// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test_mojo_connection_manager.h"

#include <fcntl.h>

#include <optional>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/common/chrome_paths.h"
#include "components/account_manager_core/account.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace crosapi {

namespace {

// TODO(crbug.com/40147422): Refactor the code to share with ARC.
base::ScopedFD CreateSocketForTesting(const base::FilePath& socket_path) {
  auto endpoint = mojo::NamedPlatformChannel({socket_path.value()});
  base::ScopedFD socket_fd =
      endpoint.TakeServerEndpoint().TakePlatformHandle().TakeFD();
  if (!socket_fd.is_valid()) {
    PLOG(ERROR) << "Failed to create socket file: " << socket_path;
    return socket_fd;
  }

  if (!base::SetPosixFilePermissions(socket_path, 0660)) {
    PLOG(ERROR) << "Could not set permissions on socket file: " << socket_path;
    return base::ScopedFD();
  }

  return socket_fd;
}

}  // namespace

TestMojoConnectionManager::TestMojoConnectionManager(
    const base::FilePath& socket_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CreateSocketForTesting, socket_path),
      base::BindOnce(&TestMojoConnectionManager::OnTestingSocketCreated,
                     weak_factory_.GetWeakPtr()));
}

TestMojoConnectionManager::~TestMojoConnectionManager() = default;

void TestMojoConnectionManager::OnTestingSocketCreated(
    base::ScopedFD socket_fd) {
  if (!socket_fd.is_valid())
    return;

  testing_socket_ = std::move(socket_fd);
  testing_socket_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      testing_socket_.get(),
      base::BindRepeating(&TestMojoConnectionManager::OnTestingSocketAvailable,
                          weak_factory_.GetWeakPtr()));
}

void TestMojoConnectionManager::OnTestingSocketAvailable() {
  base::ScopedFD connection_fd;
  if (!mojo::AcceptSocketConnection(testing_socket_.get(), &connection_fd,
                                    /* check_peer_user = */ false) ||
      !connection_fd.is_valid()) {
    LOG(ERROR) << "Failed to Accept the socket";
    return;
  }

  mojo::PlatformChannel channel;
  CrosapiManager::Get()->SendInvitation(
      channel.TakeLocalEndpoint(), base::BindOnce([]() {
        LOG(WARNING) << "Mojo to lacros-chrome is disconnected";
      }));

  base::ScopedFD startup_fd = browser_util::CreateStartupData(
      browser_util::InitialBrowserAction(
          mojom::InitialBrowserAction::kUseStartupPreference),
      /*is_keep_alive_enabled=*/false, std::nullopt);
  if (!startup_fd.is_valid()) {
    LOG(ERROR) << "Failed to create startup data";
    return;
  }

  std::vector<base::ScopedFD> fds;
  fds.push_back(std::move(startup_fd));
  fds.push_back(channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD());

  // Version of protocol Chrome is using.
  uint8_t protocol_version = 1;
  struct iovec iov[] = {{&protocol_version, sizeof(protocol_version)}};
  ssize_t result = mojo::SendmsgWithHandles(connection_fd.get(), iov,
                                            sizeof(iov) / sizeof(iov[0]), fds);
  if (result == -1) {
    PLOG(ERROR) << "Failed to send file descriptors to the socket";
    return;
  }
}

}  // namespace crosapi
