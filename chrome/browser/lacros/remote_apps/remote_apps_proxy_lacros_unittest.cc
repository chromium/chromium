// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros.h"

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/test_event_router.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

using chromeos::remote_apps::mojom::AddAppResultPtr;
using chromeos::remote_apps::mojom::AddFolderResultPtr;
using chromeos::remote_apps::mojom::RemoteAppLaunchObserver;
using chromeos::remote_apps::mojom::RemoteApps;
using chromeos::remote_apps::mojom::RemoteAppsLacrosBridge;

using AddAppCallback = base::OnceCallback<void(AddAppResultPtr)>;
using AddFolderCallback = base::OnceCallback<void(AddFolderResultPtr)>;
using DeleteAppCallback =
    base::OnceCallback<void(const std::optional<std::string>&)>;
using SortLauncherWithRemoteAppsFirstCallback =
    base::OnceCallback<void(const std::optional<std::string>&)>;
using SetPinnedAppsCallback =
    base::OnceCallback<void(const std::optional<std::string>&)>;

using testing::_;

namespace {

class TestRemoteAppsLacrosBridge : public RemoteAppsLacrosBridge,
                                   public RemoteApps {
 public:
  // RemoteAppsLacrosBridge:
  void BindRemoteAppsAndAppLaunchObserverForLacros(
      mojo::PendingReceiver<RemoteApps> remote_apps,
      mojo::PendingRemote<RemoteAppLaunchObserver> observer) override {
    remote_apps_receiver_.Bind(std::move(remote_apps));
    observer_remote_.Bind(std::move(observer));
  }

  void LaunchRemoteApp(const std::string& app_id,
                       const std::string& source_id) {
    observer_remote_->OnRemoteAppLaunched(app_id, source_id);
  }

  // RemoteApps:
  MOCK_METHOD(void,
              AddFolder,
              (const std::string&, bool, AddFolderCallback),
              (override));
  MOCK_METHOD(void,
              AddApp,
              (const std::string&,
               const std::string&,
               const std::string&,
               const GURL&,
               bool,
               AddAppCallback),
              (override));
  MOCK_METHOD(void,
              DeleteApp,
              (const std::string&, DeleteAppCallback),
              (override));
  MOCK_METHOD(void,
              SortLauncherWithRemoteAppsFirst,
              (SortLauncherWithRemoteAppsFirstCallback),
              (override));
  MOCK_METHOD(void,
              SetPinnedApps,
              (const std::vector<std::string>&, SetPinnedAppsCallback),
              (override));

 private:
  mojo::Receiver<RemoteApps> remote_apps_receiver_{this};
  mojo::Remote<RemoteAppLaunchObserver> observer_remote_;
};

class MockRemoteAppLaunchObserver : public RemoteAppLaunchObserver {
 public:
  MOCK_METHOD(void,
              OnRemoteAppLaunched,
              (const std::string&, const std::string&),
              (override));
};

constexpr char kSource[] = "source";
constexpr char kId[] = "id";
constexpr char kName[] = "name";
constexpr char kFolderId[] = "folder_id";
constexpr char kIconUrl[] = "icon_url";

}  // namespace

class RemoteAppsProxyLacrosUnittest : public testing::Test {
 public:
  RemoteAppsProxyLacrosUnittest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    CHECK(testing_profile_manager_.SetUp());
  }

  ~RemoteAppsProxyLacrosUnittest() override {}

  void SetUp() override {
    testing::Test::SetUp();

    testing_profile_ = testing_profile_manager_.CreateTestingProfile("test");
    event_router_ = extensions::CreateAndUseTestEventRouter(testing_profile_);

    bridge_remote_.Bind(bridge_receiver_.BindNewPipeAndPassRemote());
    proxy_ = RemoteAppsProxyLacros::CreateForTesting(testing_profile_,
                                                     bridge_remote_);
    // Wait for Mojo endpoints to connect.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    event_router_ = nullptr;
    testing_profile_ = nullptr;
    proxy_.reset();
    testing_profile_manager_.DeleteAllTestingProfiles();
    testing::Test::TearDown();
  }

  void SetExpectationForAddAppSuccess(const std::string& app_id,
                                      const std::string& source_id,
                                      const std::string& app_name,
                                      const std::string& folder_id,
                                      const GURL& icon_url,
                                      bool add_to_front) {
    EXPECT_CALL(remote_apps_bridge_, AddApp(source_id, app_name, folder_id,
                                            icon_url, add_to_front, _))
        .WillOnce([app_id](const std::string&, const std::string&,
                           const std::string&, GURL, bool,
                           AddAppCallback callback) {
          std::move(callback).Run(
              chromeos::remote_apps::mojom::AddAppResult::NewAppId(app_id));
        });
  }

  void SetExpectationForAddAppError(const std::string& error,
                                    const std::string& source_id,
                                    const std::string& app_name,
                                    const std::string& folder_id,
                                    const GURL& icon_url,
                                    bool add_to_front) {
    EXPECT_CALL(remote_apps_bridge_, AddApp(source_id, app_name, folder_id,
                                            icon_url, add_to_front, _))
        .WillOnce([error](const std::string&, const std::string&,
                          const std::string&, GURL, bool,
                          AddAppCallback callback) {
          std::move(callback).Run(
              chromeos::remote_apps::mojom::AddAppResult::NewError(error));
        });
  }

  void SetExpectationForAddFolderSuccess(const std::string& folder_id,
                                         const std::string& folder_name,
                                         bool add_to_front) {
    EXPECT_CALL(remote_apps_bridge_, AddFolder(folder_name, add_to_front, _))
        .WillOnce(
            [folder_id](const std::string&, bool, AddFolderCallback callback) {
              std::move(callback).Run(
                  chromeos::remote_apps::mojom::AddFolderResult::NewFolderId(
                      folder_id));
            });
  }

  void SetExpectationForAddFolderError(const std::string& error,
                                       const std::string& folder_name,
                                       bool add_to_front) {
    EXPECT_CALL(remote_apps_bridge_, AddFolder(folder_name, add_to_front, _))
        .WillOnce([error](const std::string&, bool,
                          AddFolderCallback callback) {
          std::move(callback).Run(
              chromeos::remote_apps::mojom::AddFolderResult::NewError(error));
        });
  }

  void SetExpectationForDeleteAppSuccess(const std::string& app_id) {
    EXPECT_CALL(remote_apps_bridge_, DeleteApp(app_id, _))
        .WillOnce([](const std::string&, DeleteAppCallback callback) {
          std::move(callback).Run(std::nullopt);
        });
  }

  void SetExpectationForDeleteAppError(const std::string& error,
                                       const std::string& app_id) {
    EXPECT_CALL(remote_apps_bridge_, DeleteApp(app_id, _))
        .WillOnce([error](const std::string&, DeleteAppCallback callback) {
          std::move(callback).Run(error);
        });
  }

  void SetExpectationForSortLauncherSuccess() {
    EXPECT_CALL(remote_apps_bridge_, SortLauncherWithRemoteAppsFirst(_))
        .WillOnce([](SortLauncherWithRemoteAppsFirstCallback callback) {
          std::move(callback).Run(std::nullopt);
        });
  }

  void SetExpectationForSortLauncherError(const std::string& error) {
    EXPECT_CALL(remote_apps_bridge_, SortLauncherWithRemoteAppsFirst(_))
        .WillOnce([error](SortLauncherWithRemoteAppsFirstCallback callback) {
          std::move(callback).Run(error);
        });
  }

  void SetExpectationForSetPinnedAppsSuccess(
      const std::vector<std::string>& app_ids) {
    EXPECT_CALL(remote_apps_bridge_, SetPinnedApps(app_ids, _))
        .WillOnce([](const std::vector<std::string>&,
                     SetPinnedAppsCallback callback) {
          std::move(callback).Run(std::nullopt);
        });
  }

  void SetExpectationForSetPinnedAppsError(
      const std::string& error,
      const std::vector<std::string>& app_ids) {
    EXPECT_CALL(remote_apps_bridge_, SetPinnedApps(app_ids, _))
        .WillOnce([error](const std::vector<std::string>&,
                          SetPinnedAppsCallback callback) {
          std::move(callback).Run(error);
        });
  }

 protected:
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> testing_profile_ = nullptr;
  raw_ptr<extensions::TestEventRouter> event_router_ =
      nullptr;  // Created in SetUp().
  testing::StrictMock<TestRemoteAppsLacrosBridge> remote_apps_bridge_;
  mojo::Receiver<RemoteAppsLacrosBridge> bridge_receiver_{&remote_apps_bridge_};
  mojo::Remote<RemoteAppsLacrosBridge> bridge_remote_;
  std::unique_ptr<RemoteAppsProxyLacros> proxy_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(RemoteAppsProxyLacrosUnittest, AddApp) {
  std::string app_id = "app_id";

  base::test::TestFuture<AddAppResultPtr> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  SetExpectationForAddAppSuccess(app_id, kSource, kName, kFolderId,
                                 GURL(kIconUrl), true);
  remote->AddApp(kSource, kName, kFolderId, GURL(kIconUrl), true,
                 future.GetCallback());

  AddAppResultPtr result = future.Take();
  ASSERT_TRUE(result->is_app_id());
  ASSERT_EQ(app_id, result->get_app_id());
}

TEST_F(RemoteAppsProxyLacrosUnittest, AddAppError) {
  std::string error = "error";

  base::test::TestFuture<AddAppResultPtr> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  SetExpectationForAddAppError(error, kSource, kName, kFolderId, GURL(kIconUrl),
                               true);
  remote->AddApp(kSource, kName, kFolderId, GURL(kIconUrl), true,
                 future.GetCallback());

  AddAppResultPtr result = future.Take();
  ASSERT_TRUE(result->is_error());
  ASSERT_EQ(error, result->get_error());
}

TEST_F(RemoteAppsProxyLacrosUnittest, AddFolder) {
  std::string folder_id = "folder_id";

  base::test::TestFuture<AddFolderResultPtr> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  SetExpectationForAddFolderSuccess(folder_id, kName, true);
  remote->AddFolder(kName, true, future.GetCallback());

  AddFolderResultPtr result = future.Take();
  ASSERT_TRUE(result->is_folder_id());
  ASSERT_EQ(folder_id, result->get_folder_id());
}

TEST_F(RemoteAppsProxyLacrosUnittest, AddFolderError) {
  std::string error = "error";

  base::test::TestFuture<AddFolderResultPtr> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  SetExpectationForAddFolderError(error, kName, true);
  remote->AddFolder(kName, true, future.GetCallback());

  AddFolderResultPtr result = future.Take();
  ASSERT_TRUE(result->is_error());
  ASSERT_EQ(error, result->get_error());
}

TEST_F(RemoteAppsProxyLacrosUnittest, DeleteApp) {
  base::test::TestFuture<std::optional<std::string>> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  SetExpectationForDeleteAppSuccess(kId);
  remote->DeleteApp(kId,
                    future.GetCallback<const std::optional<std::string>&>());

  ASSERT_FALSE(future.Get());
}

TEST_F(RemoteAppsProxyLacrosUnittest, DeleteAppError) {
  std::string error = "error";

  base::test::TestFuture<std::optional<std::string>> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  SetExpectationForDeleteAppError(error, kId);
  remote->DeleteApp(kId,
                    future.GetCallback<const std::optional<std::string>&>());

  std::optional<const std::string> result = future.Get();
  ASSERT_TRUE(result);
  ASSERT_EQ(error, *result);
}

TEST_F(RemoteAppsProxyLacrosUnittest, SortLauncherWithRemoteAppsFirst) {
  base::test::TestFuture<std::optional<std::string>> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  SetExpectationForSortLauncherSuccess();
  remote->SortLauncherWithRemoteAppsFirst(
      future.GetCallback<const std::optional<std::string>&>());

  ASSERT_FALSE(future.Get());
}

TEST_F(RemoteAppsProxyLacrosUnittest, SortLauncherWithRemoteAppsFirstError) {
  std::string error = "error";

  base::test::TestFuture<std::optional<std::string>> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  SetExpectationForSortLauncherError(error);
  remote->SortLauncherWithRemoteAppsFirst(
      future.GetCallback<const std::optional<std::string>&>());

  std::optional<const std::string> result = future.Get();
  ASSERT_TRUE(result);
  ASSERT_EQ(error, *result);
}

TEST_F(RemoteAppsProxyLacrosUnittest, SetPinnedApps) {
  base::test::TestFuture<std::optional<std::string>> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  std::vector<std::string> ids = {kId};
  SetExpectationForSetPinnedAppsSuccess(ids);
  remote->SetPinnedApps(
      ids, future.GetCallback<const std::optional<std::string>&>());

  ASSERT_FALSE(future.Get());
}

TEST_F(RemoteAppsProxyLacrosUnittest, SetPinnedAppsError) {
  std::string error = "error";

  base::test::TestFuture<std::optional<std::string>> future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer{&mockObserver};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      kSource, remote.BindNewPipeAndPassReceiver(),
      observer.BindNewPipeAndPassRemote());

  std::vector<std::string> ids = {kId};
  SetExpectationForSetPinnedAppsError(error, ids);
  remote->SetPinnedApps(
      ids, future.GetCallback<const std::optional<std::string>&>());

  std::optional<const std::string> result = future.Get();
  ASSERT_TRUE(result);
  ASSERT_EQ(error, *result);
}

// Tests that the `OnRemoteAppLaunched` event is only dispatched to the source
// which added the app by adding multiple apps from different sources.
TEST_F(RemoteAppsProxyLacrosUnittest, OnRemoteAppLaunched) {
  std::string source1 = "source1";
  std::string source2 = "source2";
  std::string app_id1 = "app_id1";
  std::string app_id2 = "app_id2";

  base::test::TestFuture<AddAppResultPtr> add_app_with_source1_future;
  base::test::TestFuture<AddAppResultPtr> add_app_with_source2_future;
  base::test::TestFuture<std::string>
      on_remote_app_launched_with_app_id1_future;
  base::test::TestFuture<std::string>
      on_remote_app_launched_with_app_id2_future;

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver1;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote1;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer1{&mockObserver1};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      source1, remote1.BindNewPipeAndPassReceiver(),
      observer1.BindNewPipeAndPassRemote());

  testing::StrictMock<MockRemoteAppLaunchObserver> mockObserver2;
  mojo::Remote<chromeos::remote_apps::mojom::RemoteApps> remote2;
  mojo::Receiver<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      observer2{&mockObserver2};
  proxy_->BindRemoteAppsAndAppLaunchObserver(
      source2, remote2.BindNewPipeAndPassReceiver(),
      observer2.BindNewPipeAndPassRemote());

  SetExpectationForAddAppSuccess(app_id1, source1, kName, kFolderId,
                                 GURL(kIconUrl), true);
  remote1->AddApp(source1, kName, kFolderId, GURL(kIconUrl), true,
                  add_app_with_source1_future.GetCallback());

  AddAppResultPtr result1 = add_app_with_source1_future.Take();
  ASSERT_TRUE(result1->is_app_id());
  ASSERT_EQ(app_id1, result1->get_app_id());

  SetExpectationForAddAppSuccess(app_id2, source2, kName, kFolderId,
                                 GURL(kIconUrl), true);
  remote1->AddApp(source2, kName, kFolderId, GURL(kIconUrl), true,
                  add_app_with_source2_future.GetCallback());

  AddAppResultPtr result2 = add_app_with_source2_future.Take();
  ASSERT_TRUE(result2->is_app_id());
  ASSERT_EQ(app_id2, result2->get_app_id());

  EXPECT_CALL(mockObserver1, OnRemoteAppLaunched(app_id1, source1))
      .WillOnce([&on_remote_app_launched_with_app_id1_future](
                    const std::string& app_id, const std::string& source_id) {
        on_remote_app_launched_with_app_id1_future.SetValue(app_id);
      });

  remote_apps_bridge_.LaunchRemoteApp(app_id1, source1);
  ASSERT_EQ(app_id1, on_remote_app_launched_with_app_id1_future.Get());

  EXPECT_CALL(mockObserver2, OnRemoteAppLaunched(app_id2, source2))
      .WillOnce([&on_remote_app_launched_with_app_id2_future](
                    const std::string& app_id, const std::string& source_id) {
        on_remote_app_launched_with_app_id2_future.SetValue(app_id);
      });

  remote_apps_bridge_.LaunchRemoteApp(app_id2, source2);
  ASSERT_EQ(app_id2, on_remote_app_launched_with_app_id2_future.Get());
}

}  // namespace chromeos
