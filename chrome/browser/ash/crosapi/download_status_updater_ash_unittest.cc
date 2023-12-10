// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/download_status_updater_ash.h"

#include <list>
#include <string>

#include "base/functional/callback.h"
#include "base/test/gmock_move_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/mock_download_status_updater_client.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

// Aliases.
using ::crosapi::mojom::DownloadStatusUpdater;
using ::crosapi::mojom::DownloadStatusUpdaterClient;
using ::testing::_;
using ::testing::Action;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::TestWithParam;
using ::testing::Values;

// DownloadStatusUpdaterAshCommandTest -----------------------------------------

// Enumeration of `DownloadStatusUpdaterAsh` commands to test.
enum class Command {
  kCancel,         // See `DownloadStatusUpdaterAsh::Cancel()`.
  kPause,          // See `DownloadStatusUpdaterAsh::Pause()`.
  kResume,         // See `DownloadStatusUpdaterAsh::Resume()`.
  kShowInBrowser,  // See `DownloadStatusUpdaterAsh::ShowInBrowser()`.
};

// Base class for tests of `DownloadStatusUpdaterAsh` which are parameterized by
// which `Command` to run for testing.
class DownloadStatusUpdaterAshCommandTest : public TestWithParam<Command> {
 public:
  // Callback which accepts whether a `Command` has been `handled` successfully.
  using CommandHandledCallback = base::OnceCallback<void(bool handled)>;

  // Binds a new client to the download status updater under test, with default
  // fulfillment for the `command()` under test given parameterization.
  void BindClient(
      Matcher<const std::string&> default_guid_matcher,
      Action<void(const std::string& guid, CommandHandledCallback callback)>
          default_action) {
    // Create client.
    clients_.emplace_back();

    // Add default fulfillment for `command()`.
    switch (command()) {
      case Command::kCancel:
        ON_CALL(clients_.back(), Cancel(default_guid_matcher, _))
            .WillByDefault(default_action);
        break;
      case Command::kPause:
        ON_CALL(clients_.back(), Pause(default_guid_matcher, _))
            .WillByDefault(default_action);
        break;
      case Command::kResume:
        ON_CALL(clients_.back(), Resume(default_guid_matcher, _))
            .WillByDefault(default_action);
        break;
      case Command::kShowInBrowser:
        ON_CALL(clients_.back(), ShowInBrowser(default_guid_matcher, _))
            .WillByDefault(default_action);
        break;
    }

    // Bind client.
    client_receivers_.emplace_back(&clients_.back());
    static_cast<DownloadStatusUpdater*>(download_status_updater_ash())
        ->BindClient(client_receivers_.back().BindNewPipeAndPassRemote());
  }

  // Runs the current message loop until a no-op message has been received on
  // each client interface's message pipe. This effectively ensures that any
  // messages in transit are received before returning.
  void FlushClientsForTesting() {
    for (auto& client_receiver : client_receivers_) {
      client_receiver.FlushForTesting();
    }
  }

  // Runs the `command()` under test given parameterization. Note that
  // `callback` may be run asynchronously.
  void RunCommand(const std::string& guid, CommandHandledCallback callback) {
    switch (command()) {
      case Command::kCancel:
        download_status_updater_ash()->Cancel(guid, std::move(callback));
        break;
      case Command::kPause:
        download_status_updater_ash()->Pause(guid, std::move(callback));
        break;
      case Command::kResume:
        download_status_updater_ash()->Resume(guid, std::move(callback));
        break;
      case Command::kShowInBrowser:
        download_status_updater_ash()->ShowInBrowser(guid, std::move(callback));
        break;
    }
  }

  // Returns the `Command` under test given parameterization.
  Command command() const { return GetParam(); }

  // Returns the `DownloadStatusUpdaterAsh` instance under test.
  DownloadStatusUpdaterAsh* download_status_updater_ash() {
    return &download_status_updater_ash_;
  }

 private:
  // `RunLoop`s require a task environment. Though not used explicitly,
  // this test suite has a number of `RunLoop` dependencies.
  content::BrowserTaskEnvironment task_environment_;

  // The download status updater instance under test with a testing profile,
  // which requires a browser thread environment.
  TestingProfile profile_;
  DownloadStatusUpdaterAsh download_status_updater_ash_{&profile_};

  // The collection of clients which are bound to the download status updater
  // under test. Note that clients must be explicitly bound via `BindClient()`.
  std::list<NiceMock<MockDownloadStatusUpdaterClient>> clients_;
  std::list<mojo::Receiver<DownloadStatusUpdaterClient>> client_receivers_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         DownloadStatusUpdaterAshCommandTest,
                         Values(Command::kCancel,
                                Command::kPause,
                                Command::kResume,
                                Command::kShowInBrowser),
                         [](auto& info) {
                           switch (info.param) {
                             case Command::kCancel:
                               return "Cancel";
                             case Command::kPause:
                               return "Pause";
                             case Command::kResume:
                               return "Resume";
                             case Command::kShowInBrowser:
                               return "ShowInBrowser";
                           }
                         });

// Tests -----------------------------------------------------------------------

// Verifies that `DownloadStatusUpdaterAsh` commands are working as intended.
TEST_P(DownloadStatusUpdaterAshCommandTest, Command) {
  constexpr char kGuid[] = "guid";

  // Case: No bound clients, command unhandled.
  {
    // When no clients are bound, running a download status updater command
    // should synchronously return that the command was unhandled.
    base::test::TestFuture<bool> run_command_future;
    RunCommand(kGuid, run_command_future.GetCallback());
    EXPECT_TRUE(run_command_future.IsReady());
    EXPECT_FALSE(run_command_future.Get());
  }

  // Bind multiple clients that (a) verify the `kGuid` associated with the
  // command is being propagated properly, and (b) cache the callback to run to
  // indicate whether the client handled the command successfully.
  CommandHandledCallback handled_by_client1_callback;
  BindClient(Eq(kGuid), MoveArg<1>(&handled_by_client1_callback));
  CommandHandledCallback handled_by_client2_callback;
  BindClient(Eq(kGuid), MoveArg<1>(&handled_by_client2_callback));

  // Case: Multiple bound clients, command unhandled.
  {
    // When clients are bound, running a download status updater command
    // should return whether the command was handled asynchronously.
    base::test::TestFuture<bool> run_command_future;
    RunCommand(kGuid, run_command_future.GetCallback());
    FlushClientsForTesting();
    EXPECT_FALSE(run_command_future.IsReady());

    // The command should have been dispatched to all clients.
    EXPECT_TRUE(handled_by_client1_callback);
    EXPECT_TRUE(handled_by_client2_callback);

    // All clients should indicate whether the command was handled before
    // the download status updater returns its response asynchronously.
    std::move(handled_by_client1_callback).Run(/*handled=*/false);
    FlushClientsForTesting();
    EXPECT_FALSE(run_command_future.IsReady());
    std::move(handled_by_client2_callback).Run(/*handled=*/false);
    FlushClientsForTesting();
    EXPECT_TRUE(run_command_future.IsReady());
    EXPECT_FALSE(run_command_future.Get());
  }

  // Case: Multiple bound clients, command handled.
  {
    // When clients are bound, running a download status updater command
    // should return whether the command was handled asynchronously.
    base::test::TestFuture<bool> run_command_future;
    RunCommand(kGuid, run_command_future.GetCallback());
    FlushClientsForTesting();

    // The command should have been dispatched to all clients.
    EXPECT_FALSE(run_command_future.IsReady());
    EXPECT_TRUE(handled_by_client1_callback);
    EXPECT_TRUE(handled_by_client2_callback);

    // All clients should indicate whether the command was handled before
    // the download status updater returns its response asynchronously.
    std::move(handled_by_client1_callback).Run(/*handled=*/true);
    FlushClientsForTesting();
    EXPECT_FALSE(run_command_future.IsReady());
    std::move(handled_by_client2_callback).Run(/*handled=*/false);
    FlushClientsForTesting();
    EXPECT_TRUE(run_command_future.IsReady());
    EXPECT_TRUE(run_command_future.Get());
  }
}

}  // namespace crosapi
