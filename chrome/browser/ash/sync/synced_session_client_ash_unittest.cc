// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/synced_session_client_ash.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/synced_session_client.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kTestSessionName[] = "testing";
constexpr char kTestUrl[] = "www.google.com";
constexpr char16_t kTestTitle[] = u"helloworld";
constexpr base::Time kTestModifiedTime = base::Time::FromTimeT(100);
constexpr base::Time kTestLastModifiedTimestamp = base::Time::FromTimeT(200);

std::vector<crosapi::mojom::SyncedSessionPtr> CreateTestSyncedSessions() {
  std::vector<crosapi::mojom::SyncedSessionPtr> test_sessions;
  crosapi::mojom::SyncedSessionPtr test_session =
      crosapi::mojom::SyncedSession::New();
  test_session->session_name = kTestSessionName;
  test_session->modified_time = kTestModifiedTime;
  crosapi::mojom::SyncedSessionWindowPtr test_window =
      crosapi::mojom::SyncedSessionWindow::New();
  crosapi::mojom::SyncedSessionTabPtr test_tab =
      crosapi::mojom::SyncedSessionTab::New();
  test_tab->current_navigation_url = GURL(kTestUrl);
  test_tab->last_modified_timestamp = kTestLastModifiedTimestamp;
  test_tab->current_navigation_title = kTestTitle;
  test_window->tabs.push_back(std::move(test_tab));
  test_session->windows.push_back(std::move(test_window));
  test_sessions.push_back(std::move(test_session));
  return test_sessions;
}

class TestSyncedSessionClientObserver
    : public SyncedSessionClientAsh::Observer {
 public:
  TestSyncedSessionClientObserver() = default;
  TestSyncedSessionClientObserver(const TestSyncedSessionClientObserver&) =
      delete;
  TestSyncedSessionClientObserver& operator=(
      const TestSyncedSessionClientObserver&) = delete;
  ~TestSyncedSessionClientObserver() override = default;

  void OnForeignSyncedPhoneSessionsUpdated(
      const std::vector<ForeignSyncedSessionAsh>& sessions) override {
    read_sessions_ = sessions;
  }

  void OnSessionSyncEnabledChanged(bool enabled) override {
    is_session_sync_enabled_ = enabled;
  }

  const std::vector<ForeignSyncedSessionAsh>& GetLastReadSessions() const {
    return read_sessions_;
  }

  bool IsSessionSyncEnabled() const { return is_session_sync_enabled_; }

 private:
  std::vector<ForeignSyncedSessionAsh> read_sessions_;
  bool is_session_sync_enabled_ = false;
};

class FakeCrosapiSessionSyncFaviconDelegate
    : public crosapi::mojom::SyncedSessionClientFaviconDelegate {
 public:
  FakeCrosapiSessionSyncFaviconDelegate() = default;
  FakeCrosapiSessionSyncFaviconDelegate(
      const FakeCrosapiSessionSyncFaviconDelegate&) = delete;
  FakeCrosapiSessionSyncFaviconDelegate& operator=(
      const FakeCrosapiSessionSyncFaviconDelegate&) = delete;
  ~FakeCrosapiSessionSyncFaviconDelegate() override = default;

  // crosapi::mojom::SyncedSessionClientFaviconDelegate:
  void GetFaviconImageForPageURL(
      const GURL& url,
      GetFaviconImageForPageURLCallback callback) override {
    std::move(callback).Run(*result_image_);
  }

  mojo::PendingRemote<crosapi::mojom::SyncedSessionClientFaviconDelegate>
  CreateRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void SetResultImage(gfx::ImageSkia* image) { result_image_ = image; }

 private:
  mojo::Receiver<crosapi::mojom::SyncedSessionClientFaviconDelegate> receiver_{
      this};
  raw_ptr<gfx::ImageSkia> result_image_ = nullptr;
};

gfx::ImageSkia GetTestImage() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorBLUE);
  return gfx::Image::CreateFrom1xBitmap(bitmap).AsImageSkia();
}

}  // namespace

class SyncedSessionClientAshTest : public testing::Test {
 public:
  SyncedSessionClientAshTest() = default;
  SyncedSessionClientAshTest(const SyncedSessionClientAshTest&) = delete;
  SyncedSessionClientAshTest& operator=(const SyncedSessionClientAshTest&) =
      delete;
  ~SyncedSessionClientAshTest() override = default;

  void SetUp() override {
    client_ = std::make_unique<SyncedSessionClientAsh>();
    client_remote_.Bind(client_->CreateRemote());
    test_observer_ = std::make_unique<TestSyncedSessionClientObserver>();
    client_->AddObserver(test_observer_.get());
  }

  void RequestFaviconImage(gfx::ImageSkia expected_image) {
    base::test::TestFuture<const gfx::ImageSkia&> future;
    client_->GetFaviconImageForPageURL(GURL(kTestUrl), future.GetCallback());
    EXPECT_TRUE(gfx::test::AreImagesEqual(gfx::Image(expected_image),
                                          gfx::Image(future.Get())));
  }

  SyncedSessionClientAsh* client() {
    DCHECK(client_);
    return client_.get();
  }

  mojo::Remote<crosapi::mojom::SyncedSessionClient>* client_remote() {
    return &client_remote_;
  }

  TestSyncedSessionClientObserver* test_observer() {
    DCHECK(test_observer_);
    return test_observer_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<SyncedSessionClientAsh> client_;
  mojo::Remote<crosapi::mojom::SyncedSessionClient> client_remote_;
  std::unique_ptr<TestSyncedSessionClientObserver> test_observer_;
};

TEST_F(SyncedSessionClientAshTest, OnForeignSyncedPhoneSessionsUpdated) {
  client()->OnForeignSyncedPhoneSessionsUpdated(CreateTestSyncedSessions());
  std::vector<ForeignSyncedSessionAsh> observed_sessions =
      test_observer()->GetLastReadSessions();
  ASSERT_EQ(observed_sessions.size(), 1u);
  EXPECT_EQ(observed_sessions[0].session_name, kTestSessionName);
  EXPECT_EQ(observed_sessions[0].modified_time, kTestModifiedTime);
  ASSERT_EQ(observed_sessions[0].windows.size(), 1u);
  ASSERT_EQ(observed_sessions[0].windows[0].tabs.size(), 1u);
  EXPECT_EQ(observed_sessions[0].windows[0].tabs[0].current_navigation_url,
            GURL(kTestUrl));
  EXPECT_EQ(observed_sessions[0].windows[0].tabs[0].last_modified_timestamp,
            kTestLastModifiedTimestamp);
  EXPECT_EQ(observed_sessions[0].windows[0].tabs[0].current_navigation_title,
            kTestTitle);
}

TEST_F(SyncedSessionClientAshTest, OnSessionSyncEnabledChanged) {
  client()->OnSessionSyncEnabledChanged(/*enabled=*/true);
  EXPECT_TRUE(test_observer()->IsSessionSyncEnabled());
  client()->OnSessionSyncEnabledChanged(/*enabled=*/false);
  EXPECT_FALSE(test_observer()->IsSessionSyncEnabled());
}

TEST_F(SyncedSessionClientAshTest, GetFaviconImage_NoRemote) {
  RequestFaviconImage(gfx::ImageSkia());
}

TEST_F(SyncedSessionClientAshTest, GetFaviconImage_ImagesMatch) {
  FakeCrosapiSessionSyncFaviconDelegate favicon_delegate;
  client()->SetFaviconDelegate(favicon_delegate.CreateRemote());

  gfx::ImageSkia empty_image;
  favicon_delegate.SetResultImage(&empty_image);
  RequestFaviconImage(gfx::ImageSkia());

  gfx::ImageSkia test_image = GetTestImage();
  favicon_delegate.SetResultImage(&test_image);
  RequestFaviconImage(GetTestImage());
}

}  // namespace ash
