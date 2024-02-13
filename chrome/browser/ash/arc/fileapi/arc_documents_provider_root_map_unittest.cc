// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kTestRootId[] = "abc_root";

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return ArcFileSystemOperationRunner::CreateForTesting(
      context, ArcServiceManager::Get()->arc_bridge_service());
}

}  // namespace

class ArcDocumentsProviderRootMapTest : public testing::Test {
 public:
  ArcDocumentsProviderRootMapTest() = default;
  ArcDocumentsProviderRootMapTest(const ArcDocumentsProviderRootMapTest&) =
      delete;
  ArcDocumentsProviderRootMapTest& operator=(
      const ArcDocumentsProviderRootMapTest&) = delete;
  ~ArcDocumentsProviderRootMapTest() override = default;

 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    SetUpARC();
    arc_documents_provider_root_map_ =
        ArcDocumentsProviderRootMap::GetForBrowserContext(profile_.get());
  }

  void TearDown() override {
    arc_documents_provider_root_map_->Shutdown();
    TearDownARC();
    profile_.reset();
  }

  ArcDocumentsProviderRootMap* GetRootMap() const {
    return arc_documents_provider_root_map_.get();
  }

 private:
  void SetUpARC() {
    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_service_manager_->set_browser_context(profile_.get());
    ArcFileSystemOperationRunner::GetFactory()->SetTestingFactoryAndUse(
        profile_.get(),
        base::BindRepeating(&CreateFileSystemOperationRunnerForTesting));
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
    WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());
    ASSERT_TRUE(fake_file_system_.InitCalled());
  }

  void TearDownARC() {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);
    arc_service_manager_->set_browser_context(nullptr);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  FakeFileSystemInstance fake_file_system_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  raw_ptr<ArcDocumentsProviderRootMap, DanglingUntriaged>
      arc_documents_provider_root_map_;
};

TEST_F(ArcDocumentsProviderRootMapTest, Lookup) {
  ArcDocumentsProviderRoot* images_root =
      GetRootMap()->Lookup(kMediaDocumentsProviderAuthority, kImagesRootId);
  EXPECT_EQ(images_root->authority_, kMediaDocumentsProviderAuthority);
  EXPECT_EQ(images_root->root_id_, kImagesRootId);
  ArcDocumentsProviderRoot* audio_root =
      GetRootMap()->Lookup(kMediaDocumentsProviderAuthority, kAudioRootId);
  EXPECT_EQ(audio_root->authority_, kMediaDocumentsProviderAuthority);
  EXPECT_EQ(audio_root->root_id_, kAudioRootId);
  ArcDocumentsProviderRoot* videos_root =
      GetRootMap()->Lookup(kMediaDocumentsProviderAuthority, kVideosRootId);
  EXPECT_EQ(videos_root->authority_, kMediaDocumentsProviderAuthority);
  EXPECT_EQ(videos_root->root_id_, kVideosRootId);
  ArcDocumentsProviderRoot* documents_root =
      GetRootMap()->Lookup(kMediaDocumentsProviderAuthority, kDocumentsRootId);
  EXPECT_EQ(documents_root->authority_, kMediaDocumentsProviderAuthority);
  EXPECT_EQ(documents_root->root_id_, kDocumentsRootId);

  EXPECT_EQ(GetRootMap()->Lookup(kMediaDocumentsProviderAuthority, kTestRootId),
            nullptr);
}

TEST_F(ArcDocumentsProviderRootMapTest, RegisterRoot) {
  EXPECT_EQ(GetRootMap()->Lookup(kMediaDocumentsProviderAuthority, kTestRootId),
            nullptr);
  GetRootMap()->RegisterRoot(kMediaDocumentsProviderAuthority, kTestRootId,
                             kTestRootId, false, {});
  EXPECT_NE(GetRootMap()->Lookup(kMediaDocumentsProviderAuthority, kTestRootId),
            nullptr);
}

TEST_F(ArcDocumentsProviderRootMapTest, UnregisterRoot) {
  GetRootMap()->RegisterRoot(kMediaDocumentsProviderAuthority, kTestRootId,
                             kTestRootId, true, {});
  EXPECT_NE(GetRootMap()->Lookup(kMediaDocumentsProviderAuthority, kTestRootId),
            nullptr);
  GetRootMap()->UnregisterRoot(kMediaDocumentsProviderAuthority, kTestRootId);
  EXPECT_EQ(GetRootMap()->Lookup(kMediaDocumentsProviderAuthority, kTestRootId),
            nullptr);
}

}  // namespace arc
