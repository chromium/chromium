// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/test_mojo_connection_manager.h"

#include <fcntl.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/crosapi/ash_chrome_service_impl.h"
#include "chrome/browser/chromeos/crosapi/browser_util.h"
#include "chromeos/constants/chromeos_switches.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/system/invitation.h"

namespace crosapi {

namespace {

class FakeEnvironmentProvider : public EnvironmentProvider {
  crosapi::mojom::SessionType GetSessionType() override {
    return crosapi::mojom::SessionType::kRegularSession;
  }
};

// TODO(crbug.com/1124494): Refactor the code to share with ARC.
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
    const base::FilePath& socket_path)
    : environment_provider_(std::make_unique<FakeEnvironmentProvider>()) {
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

  // TODO(crbug.com/1124490): Support multiple mojo connections from lacros.
  mojo::PlatformChannel channel;
  lacros_chrome_service_ = browser_util::SendMojoInvitationToLacrosChrome(
      environment_provider_.get(), channel.TakeLocalEndpoint(),
      base::BindOnce(&TestMojoConnectionManager::OnMojoDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(
          &TestMojoConnectionManager::OnAshChromeServiceReceiverReceived,
          weak_factory_.GetWeakPtr()));

  std::vector<base::ScopedFD> fds;
  fds.emplace_back(channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD());

  // Version of protocol Chrome is using.
  uint8_t protocol_version = 0;
  struct iovec iov[] = {{&protocol_version, sizeof(protocol_version)}};
  ssize_t result = mojo::SendmsgWithHandles(connection_fd.get(), iov,
                                            sizeof(iov) / sizeof(iov[0]), fds);
  if (result == -1) {
    PLOG(ERROR) << "Failed to send file descriptors to the socket";
    return;
  }
}

void TestMojoConnectionManager::OnAshChromeServiceReceiverReceived(
    mojo::PendingReceiver<crosapi::mojom::AshChromeService> pending_receiver) {
  ash_chrome_service_ =
      std::make_unique<AshChromeServiceImpl>(std::move(pending_receiver));
  LOG(INFO) << "Connection to lacros-chrome is established.";
}

void TestMojoConnectionManager::OnMojoDisconnected() {
  lacros_chrome_service_.reset();
  ash_chrome_service_ = nullptr;
  LOG(WARNING) << "Mojo to lacros-chrome is disconnected.";
}

}  // namespace crosapi
