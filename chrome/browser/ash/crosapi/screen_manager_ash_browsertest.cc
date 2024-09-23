// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/crosapi/screen_manager_ash.h"

#include <memory>

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/aura/window.h"
#include "ui/display/test/display_manager_test_api.h"

namespace crosapi {
namespace {

// This class tests that the ash-chrome implementation of the screen manager
// crosapi works properly.
class ScreenManagerAshBrowserTest : public InProcessBrowserTest {
 protected:
  using SMRemote = mojo::Remote<mojom::ScreenManager>;
  using SMPendingRemote = mojo::PendingRemote<mojom::ScreenManager>;
  using SMPendingReceiver = mojo::PendingReceiver<mojom::ScreenManager>;

  ScreenManagerAshBrowserTest() = default;

  ScreenManagerAshBrowserTest(const ScreenManagerAshBrowserTest&) = delete;
  ScreenManagerAshBrowserTest& operator=(const ScreenManagerAshBrowserTest&) =
      delete;

  ~ScreenManagerAshBrowserTest() override {
    background_sequence_->DeleteSoon(FROM_HERE,
                                     std::move(screen_manager_remote_));
  }

  void SetUpOnMainThread() override {
    // The implementation of screen manager is affine to this sequence.
    screen_manager_ = std::make_unique<ScreenManagerAsh>();

    SMPendingRemote pending_remote;
    SMPendingReceiver pending_receiver =
        pending_remote.InitWithNewPipeAndPassReceiver();

    // Bind the implementation of ScreenManager to this sequence.
    screen_manager_->BindReceiver(std::move(pending_receiver));

    // Bind the remote to a background sequence. This is necessary because the
    // screen manager API is synchronous and blocks the calling sequence.
    background_sequence_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

    // We construct the remote on this sequence for simplicity. All subsequent
    // invocations, including destruction, are from the background sequence.
    screen_manager_remote_ = std::make_unique<SMRemote>();
    auto bind_background = base::BindOnce(
        [](SMRemote* remote, SMPendingRemote pending_remote) {
          remote->Bind(std::move(pending_remote));
        },
        screen_manager_remote_.get(), std::move(pending_remote));
    background_sequence_->PostTask(FROM_HERE, std::move(bind_background));
  }

  void PostTaskAndWait(base::OnceClosure closure) {
    base::test::TestFuture<void> future;
    background_sequence_->PostTaskAndReply(FROM_HERE, std::move(closure),
                                           future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  // Affine to main sequence.
  std::unique_ptr<ScreenManagerAsh> screen_manager_;

  // A sequence that is allowed to block.
  scoped_refptr<base::SequencedTaskRunner> background_sequence_;

  // Affine to background sequence.
  std::unique_ptr<SMRemote> screen_manager_remote_;
};

IN_PROC_BROWSER_TEST_F(ScreenManagerAshBrowserTest, ScreenCapturer) {
  bool success;
  SkBitmap snapshot;

  // Take a snapshot on a background sequence. The call is blocking, so when it
  // finishes, we can also unblock the main thread.
  auto take_snapshot_background = base::BindOnce(
      [](SMRemote* remote, bool* success, SkBitmap* snapshot) {
        mojo::Remote<mojom::SnapshotCapturer> capturer;
        (*remote)->GetScreenCapturer(capturer.BindNewPipeAndPassReceiver());

        {
          mojo::ScopedAllowSyncCallForTesting allow_sync;

          std::vector<mojom::SnapshotSourcePtr> screens;
          capturer->ListSources(&screens);

          // There should be at least one screen!
          ASSERT_LE(1u, screens.size());

          capturer->TakeSnapshot(screens[0]->id, success, snapshot);
        }
      },
      screen_manager_remote_.get(), &success, &snapshot);
  PostTaskAndWait(std::move(take_snapshot_background));

  // Check that the IPC succeeded.
  ASSERT_TRUE(success);

  // Check that the screenshot has the right dimensions.
  aura::Window* primary_window =
      browser()->window()->GetNativeWindow()->GetRootWindow();
  EXPECT_EQ(int{snapshot.width()}, primary_window->bounds().width());
  EXPECT_EQ(int{snapshot.height()}, primary_window->bounds().height());
}

IN_PROC_BROWSER_TEST_F(ScreenManagerAshBrowserTest,
                       ScreenCapturer_MultipleDisplays) {
  bool success[2];
  SkBitmap snapshot[2];

  display::test::DisplayManagerTestApi(ash::ShellTestApi().display_manager())
      .UpdateDisplay("400x300,100x500");

  // Take a snapshot on a background sequence. The call is blocking, so when it
  // finishes, we can also unblock the main thread.
  auto take_snapshot_background = base::BindOnce(
      [](SMRemote* remote, bool success[2], SkBitmap snapshot[2]) {
        mojo::Remote<mojom::SnapshotCapturer> capturer;
        (*remote)->GetScreenCapturer(capturer.BindNewPipeAndPassReceiver());

        {
          mojo::ScopedAllowSyncCallForTesting allow_sync;

          std::vector<mojom::SnapshotSourcePtr> screens;
          capturer->ListSources(&screens);

          // There should be exactly two screens!
          ASSERT_EQ(2u, screens.size());

          capturer->TakeSnapshot(screens[0]->id, &success[0], &snapshot[0]);
          capturer->TakeSnapshot(screens[1]->id, &success[1], &snapshot[1]);
        }
      },
      screen_manager_remote_.get(), success, snapshot);
  PostTaskAndWait(std::move(take_snapshot_background));

  // Check that the IPCs succeeded.
  ASSERT_TRUE(success[0]);
  ASSERT_TRUE(success[1]);

  // Check that the screenshots have the right dimensions.
  EXPECT_EQ(400, int{snapshot[0].width()});
  EXPECT_EQ(300, int{snapshot[0].height()});
  EXPECT_EQ(100, int{snapshot[1].width()});
  EXPECT_EQ(500, int{snapshot[1].height()});
}

IN_PROC_BROWSER_TEST_F(ScreenManagerAshBrowserTest, WindowCapturer) {
  bool success;
  SkBitmap snapshot;

  // Take a snapshot on a background sequence. The call is blocking, so when it
  // finishes, we can also unblock the main thread.
  auto take_snapshot_background = base::BindOnce(
      [](SMRemote* remote, bool* success, SkBitmap* snapshot) {
        mojo::Remote<mojom::SnapshotCapturer> capturer;
        (*remote)->GetWindowCapturer(capturer.BindNewPipeAndPassReceiver());

        {
          mojo::ScopedAllowSyncCallForTesting allow_sync;

          std::vector<mojom::SnapshotSourcePtr> windows;
          capturer->ListSources(&windows);

          // There should be at least one window!
          ASSERT_LE(1u, windows.size());

          capturer->TakeSnapshot(windows[0]->id, success, snapshot);
        }
      },
      screen_manager_remote_.get(), &success, &snapshot);
  PostTaskAndWait(std::move(take_snapshot_background));

  // Check that the IPC succeeded.
  ASSERT_TRUE(success);

  // Check that the screenshot has the right dimensions.
  aura::Window* window = browser()->window()->GetNativeWindow();
  EXPECT_EQ(int{snapshot.width()}, window->bounds().width());
  EXPECT_EQ(int{snapshot.height()}, window->bounds().height());
}

}  // namespace
}  // namespace crosapi
