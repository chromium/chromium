// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_image_loader_client.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

// Information about the component used in tests (not test-only; the component
// is OK to change, as long as its config still satisfies test assumptions).
constexpr char kTestComponentName[] = "demo-mode-resources";
constexpr char kTestComponentValidMinEnvVersion[] = "1.0";
constexpr char kTestComponentInvalidMinEnvVersion[] = "0.0.1";
constexpr char kTestComponentMountPath[] =
    "/run/imageloader/demo-mode-resources";

MATCHER_P(CrxComponentWithName, name, "") {
  return arg.name == name;
}

// Used as a callback to CrOSComponentManager::Load callback - it records the
// callback params to |result_out| and |mount_path_out|.
void RecordLoadResult(base::Optional<CrOSComponentManager::Error>* result_out,
                      base::FilePath* mount_path_out,
                      CrOSComponentManager::Error reported_result,
                      const base::FilePath& reported_mount_path) {
  *result_out = reported_result;
  *mount_path_out = reported_mount_path;
}

// Wraps update_client::Callback inside update_client::CrxInstaller::Install
// callback. It expects a success result to be reported.
void WrapInstallerCallback(update_client::Callback callback,
                           const update_client::CrxInstaller::Result& result) {
  EXPECT_EQ(0, result.error);
  std::move(callback).Run(update_client::Error::NONE);
}

class TestUpdater : public OnDemandUpdater {
 public:
  TestUpdater() = default;
  ~TestUpdater() override = default;

  // Whether has a pending update request (either foreground or background).
  bool HasPendingUpdate(const std::string& name) {
    return base::Contains(background_updates_, name) ||
           base::Contains(foreground_updates_, name);
  }

  // Finishes a foreground update request. Returns false if there is no pending
  // foreground update request for the component.
  // |name|: Component name.
  // |error|: The error code that the update request should report.
  // |unpacked_path|: On success, the path from which the component should be
  //     installed.
  bool FinishForegroundUpdate(const std::string& name,
                              update_client::Error error,
                              const base::FilePath& unpacked_path) {
    return FinishUpdate(name, error, unpacked_path, &foreground_updates_);
  }

  // Finishes a background update request. Returns false if there is no pending
  // foreground update request for the component.
  // |name|: Component name.
  // |error|: The error code that the update request should report.
  // |unpacked_path|: On success, the path from which the component should be
  //     installed.
  bool FinishBackgroundUpdate(const std::string& name,
                              update_client::Error error,
                              const base::FilePath& unpacked_path) {
    return FinishUpdate(name, error, unpacked_path, &background_updates_);
  }

  // Registers a CRX component for updates.
  bool RegisterComponent(const update_client::CrxComponent& component) {
    component_installers_[component.name] = component.installer;
    component_id_to_name_[update_client::GetCrxComponentID(component)] =
        component.name;
    return true;
  }

 private:
  // OnDemandUpdater:
  void OnDemandUpdate(const std::string& id,
                      OnDemandUpdater::Priority priority,
                      Callback callback) override {
    const std::string& name = component_id_to_name_[id];
    ASSERT_FALSE(name.empty());
    // Technically, this is a supported use case for background updates, but not
    // needed for this tests.
    ASSERT_FALSE(HasPendingUpdate(name));

    if (priority == OnDemandUpdater::Priority::BACKGROUND) {
      background_updates_.emplace(name, std::move(callback));
    } else {
      foreground_updates_.emplace(name, std::move(callback));
    }
  }

  // Shared implementation for FinishForegroundUpdate() and
  // FinishBackgroundUpdate().
  bool FinishUpdate(const std::string& name,
                    update_client::Error error,
                    const base::FilePath& unpacked_path,
                    std::map<std::string, Callback>* updates) {
    auto it = updates->find(name);
    if (it == updates->end())
      return false;

    Callback callback = std::move(it->second);
    updates->erase(it);

    if (error != update_client::Error::NONE) {
      std::move(callback).Run(error);
      return true;
    }

    scoped_refptr<update_client::CrxInstaller> installer =
        component_installers_[name];
    if (!installer)
      return false;

    base::PostTask(
        FROM_HERE, {base::ThreadPool(), base::MayBlock()},
        base::BindOnce(
            &update_client::CrxInstaller::Install, installer, unpacked_path, "",
            base::BindOnce(&WrapInstallerCallback, std::move(callback))));
    return true;
  }

  // Pending background updates per component.
  std::map<std::string, Callback> background_updates_;
  // Pending foreground updates per component.
  std::map<std::string, Callback> foreground_updates_;

  // Maps a component name to the component's registered CRX installer.
  std::map<std::string, scoped_refptr<update_client::CrxInstaller>>
      component_installers_;
  // Maps a registered component ID to the component name.
  std::map<std::string, std::string> component_id_to_name_;

  DISALLOW_COPY_AND_ASSIGN(TestUpdater);
};

}  // namespace

class CrOSComponentInstallerTest : public testing::Test {
 public:
  CrOSComponentInstallerTest()
      : user_manager_(std::make_unique<chromeos::FakeChromeUserManager>()) {}

  void SetUp() override {
    ASSERT_TRUE(base_component_paths_.CreateUniqueTempDir());

    preinstalled_cros_components_ = base_component_paths_.GetPath()
                                        .AppendASCII("preinstalled")
                                        .AppendASCII("cros-components");
    preinstalled_components_path_override_ =
        std::make_unique<base::ScopedPathOverride>(
            chromeos::DIR_PREINSTALLED_COMPONENTS,
            preinstalled_cros_components_.DirName());

    user_cros_components_ =
        base_component_paths_.GetPath().AppendASCII("user").AppendASCII(
            "cros-components");
    user_components_path_override_ = std::make_unique<base::ScopedPathOverride>(
        chrome::DIR_USER_DATA, user_cros_components_.DirName());

    tmp_unpack_dir_ = base_component_paths_.GetPath().AppendASCII("tmp_unpack");

    auto fake_image_loader_client =
        std::make_unique<chromeos::FakeImageLoaderClient>();
    image_loader_client_ = fake_image_loader_client.get();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetImageLoaderClient(
        std::move(fake_image_loader_client));
  }

  void TearDown() override {
    image_loader_client_ = nullptr;
    chromeos::DBusThreadManager::Shutdown();
    preinstalled_components_path_override_.reset();
    user_components_path_override_.reset();
  }

 protected:
  // Gets expected path for a user installed component with a specific version.
  base::FilePath GetInstalledComponentPath(const std::string& name,
                                           const std::string& version) {
    return user_cros_components_.AppendASCII(name).AppendASCII(version);
  }

  // Creates a fake "user" installed component.
  // On success, it returns the path at which the component was created, nullopt
  // otherwise.
  base::Optional<base::FilePath> CreateInstalledComponent(
      const std::string& name,
      const std::string& version,
      const std::string& min_env_version) {
    return CreateComponentAtPath(GetInstalledComponentPath(name, version), name,
                                 version, min_env_version);
  }

  // Creates a fake component at a pre-installed component path.
  // On success, it returns the path at which the component was created, nullopt
  // otherwise.
  base::Optional<base::FilePath> CreatePreinstalledComponent(
      const std::string& name,
      const std::string& version,
      const std::string& min_env_version) {
    return CreateComponentAtPath(
        preinstalled_cros_components_.AppendASCII(name), name, version,
        min_env_version);
  }

  // Creates a fake component at a temporary path from which the component will
  // be installed as a user-installed component by the test OnDemandUpdater.
  // On success, it returns the path at which the component was created, nullopt
  // otherwise.
  base::Optional<base::FilePath> CreateUnpackedComponent(
      const std::string& name,
      const std::string& version,
      const std::string& min_env_version) {
    return CreateComponentAtPath(
        tmp_unpack_dir_.AppendASCII(name).AppendASCII(version), name, version,
        min_env_version);
  }

  // Creates a mock ComponentUpdateService. It sets the service up to expect a
  // single registration request for the component |component_name|, and to
  // redirect on-demand update requests to |updater|.
  std::unique_ptr<MockComponentUpdateService>
  CreateUpdateServiceForSingleRegistration(const std::string& component_name,
                                           TestUpdater* updater) {
    auto service = std::make_unique<MockComponentUpdateService>();
    EXPECT_CALL(*service,
                RegisterComponent(CrxComponentWithName(component_name)))
        .Times(1)
        .WillOnce(testing::Invoke(updater, &TestUpdater::RegisterComponent));

    EXPECT_CALL(*service, GetOnDemandUpdater())
        .WillRepeatedly(testing::ReturnRef(*updater));
    return service;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  chromeos::FakeImageLoaderClient* image_loader_client() {
    return image_loader_client_;
  }

  // Verify that cros_component_manager successfully loaded a component
  // |component_name|.
  // |load_result|: The result reported by CrOSComponentManager::Load().
  // |component_install_path|: The path at which the component is expected to be
  //     installed.
  void VerifyComponentLoaded(
      const CrOSComponentManager& cros_component_manager,
      const std::string& component_name,
      base::Optional<CrOSComponentManager::Error> load_result,
      const base::FilePath& component_install_path) {
    ASSERT_TRUE(load_result.has_value());
    ASSERT_EQ(CrOSComponentManager::Error::NONE, load_result.value());

    EXPECT_EQ(component_install_path,
              cros_component_manager.GetCompatiblePath(component_name));
    EXPECT_TRUE(image_loader_client()->IsLoaded(component_name));
    EXPECT_EQ(component_install_path,
              image_loader_client()->GetComponentInstallPath(component_name));
  }

 private:
  // Creates a fake component at the specified path. Returns the target path on
  // success, nullopt otherwise.
  base::Optional<base::FilePath> CreateComponentAtPath(
      const base::FilePath& path,
      const std::string& name,
      const std::string& version,
      const std::string& min_env_version) {
    if (!base::CreateDirectory(path))
      return base::nullopt;

    const std::string manifest_template = R"({
        "name": "%s",
        "version": "%s",
        "min_env_version": "%s"
    })";
    const std::string manifest =
        base::StringPrintf(manifest_template.c_str(), name.c_str(),
                           version.c_str(), min_env_version.c_str());
    if (base::WriteFile(path.AppendASCII("manifest.json"), manifest.data(),
                        manifest.size()) != static_cast<int>(manifest.size())) {
      return base::nullopt;
    }

    return base::make_optional(path);
  }

  content::BrowserTaskEnvironment task_environment_;

  user_manager::ScopedUserManager user_manager_;

  // Image loader client that is active during the test. Owned by
  // chromeos::DBusThreadManager.
  chromeos::FakeImageLoaderClient* image_loader_client_ = nullptr;

  base::ScopedTempDir base_component_paths_;

  std::unique_ptr<base::ScopedPathOverride>
      preinstalled_components_path_override_;
  base::FilePath preinstalled_cros_components_;

  std::unique_ptr<base::ScopedPathOverride> user_components_path_override_;
  base::FilePath user_cros_components_;

  base::FilePath tmp_unpack_dir_;

  DISALLOW_COPY_AND_ASSIGN(CrOSComponentInstallerTest);
};

class MockCrOSComponentInstallerPolicy : public CrOSComponentInstallerPolicy {
 public:
  explicit MockCrOSComponentInstallerPolicy(const ComponentConfig& config)
      : CrOSComponentInstallerPolicy(config, nullptr) {}
  MOCK_METHOD2(IsCompatible,
               bool(const std::string& env_version_str,
                    const std::string& min_env_version_str));
};

TEST_F(CrOSComponentInstallerTest, CompatibleCrOSComponent) {
  component_updater::CrOSComponentInstaller cros_component_manager(nullptr,
                                                                   nullptr);

  const std::string kComponent = "a";
  EXPECT_FALSE(cros_component_manager.IsCompatible(kComponent));
  EXPECT_EQ(cros_component_manager.GetCompatiblePath(kComponent).value(),
            std::string());

  const base::FilePath kPath("/component/path/v0");
  cros_component_manager.RegisterCompatiblePath(kComponent, kPath);
  EXPECT_TRUE(cros_component_manager.IsCompatible(kComponent));
  EXPECT_EQ(cros_component_manager.GetCompatiblePath(kComponent), kPath);
  cros_component_manager.UnregisterCompatiblePath(kComponent);
  EXPECT_FALSE(cros_component_manager.IsCompatible(kComponent));
}

TEST_F(CrOSComponentInstallerTest, CompatibilityOK) {
  ComponentConfig config{"a", "2.1", ""};
  MockCrOSComponentInstallerPolicy policy(config);
  EXPECT_CALL(policy, IsCompatible(testing::_, testing::_)).Times(1);
  base::Version version;
  base::FilePath path;
  std::unique_ptr<base::DictionaryValue> manifest =
      std::make_unique<base::DictionaryValue>();
  manifest->SetString("min_env_version", "2.1");
  policy.ComponentReady(version, path, std::move(manifest));
}

TEST_F(CrOSComponentInstallerTest, CompatibilityMissingManifest) {
  ComponentConfig config{"a", "2.1", ""};
  MockCrOSComponentInstallerPolicy policy(config);
  EXPECT_CALL(policy, IsCompatible(testing::_, testing::_)).Times(0);
  base::Version version;
  base::FilePath path;
  std::unique_ptr<base::DictionaryValue> manifest =
      std::make_unique<base::DictionaryValue>();
  policy.ComponentReady(version, path, std::move(manifest));
}

TEST_F(CrOSComponentInstallerTest, IsCompatibleOrNot) {
  ComponentConfig config{"", "", ""};
  CrOSComponentInstallerPolicy policy(config, nullptr);
  EXPECT_TRUE(policy.IsCompatible("1.0", "1.0"));
  EXPECT_TRUE(policy.IsCompatible("1.1", "1.0"));
  EXPECT_FALSE(policy.IsCompatible("1.0", "1.1"));
  EXPECT_FALSE(policy.IsCompatible("1.0", "2.0"));
  EXPECT_FALSE(policy.IsCompatible("1.c", "1.c"));
  EXPECT_FALSE(policy.IsCompatible("1", "1.1"));
  EXPECT_TRUE(policy.IsCompatible("1.1.1", "1.1"));
}

TEST_F(CrOSComponentInstallerTest, RegisterComponent) {
  auto cus = std::make_unique<MockComponentUpdateService>();
  ComponentConfig config{
      "star-cups-driver", "1.1",
      "6d24de30f671da5aee6d463d9e446cafe9ddac672800a9defe86877dcde6c466"};
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(1);
  component_updater::CrOSComponentInstaller cros_component_manager(nullptr,
                                                                   cus.get());
  cros_component_manager.Register(config, base::OnceClosure());
  RunUntilIdle();
}

TEST_F(CrOSComponentInstallerTest, LoadPreinstalledComponent_Skip_Mount) {
  base::Optional<base::FilePath> install_path = CreatePreinstalledComponent(
      kTestComponentName, "1.0", kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(install_path.has_value());

  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kSkip,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  ASSERT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  VerifyComponentLoaded(cros_component_manager, kTestComponentName, load_result,
                        install_path.value());
  EXPECT_EQ(base::FilePath(kTestComponentMountPath), mount_path);
}

TEST_F(CrOSComponentInstallerTest,
       LoadInstalledComponentWhenOlderPreinstalledVersionExists_Skip_Mount) {
  base::Optional<base::FilePath> preinstalled_path =
      CreatePreinstalledComponent(kTestComponentName, "1.0",
                                  kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(preinstalled_path.has_value());

  base::Optional<base::FilePath> install_path = CreateInstalledComponent(
      kTestComponentName, "2.0", kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(install_path.has_value());

  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kSkip,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  ASSERT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  VerifyComponentLoaded(cros_component_manager, kTestComponentName, load_result,
                        install_path.value());
  EXPECT_EQ(base::FilePath(kTestComponentMountPath), mount_path);
}

TEST_F(CrOSComponentInstallerTest, LoadInstalledComponent) {
  base::Optional<base::FilePath> install_path = CreateInstalledComponent(
      kTestComponentName, "2.0", kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(install_path.has_value());

  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kSkip,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  ASSERT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  VerifyComponentLoaded(cros_component_manager, kTestComponentName, load_result,
                        install_path.value());
  EXPECT_EQ(base::FilePath(kTestComponentMountPath), mount_path);
}

TEST_F(CrOSComponentInstallerTest, LoadNonInstalledComponent_Skip_Mount) {
  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kSkip,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  ASSERT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  ASSERT_TRUE(load_result.has_value());
  EXPECT_EQ(CrOSComponentManager::Error::NOT_FOUND, load_result.value());
  EXPECT_TRUE(mount_path.empty());

  EXPECT_TRUE(
      cros_component_manager.GetCompatiblePath(kTestComponentName).empty());

  EXPECT_FALSE(image_loader_client()->IsLoaded(kTestComponentName));
}

TEST_F(CrOSComponentInstallerTest, LoadObsoleteInstalledComponent_Skip_Mount) {
  base::Optional<base::FilePath> old_install_path = CreateInstalledComponent(
      kTestComponentName, "0.5", kTestComponentInvalidMinEnvVersion);
  ASSERT_TRUE(old_install_path.has_value());
  base::Optional<base::FilePath> old_preinstall_path = CreateInstalledComponent(
      kTestComponentName, "0.5", kTestComponentInvalidMinEnvVersion);
  ASSERT_TRUE(old_preinstall_path.has_value());

  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kSkip,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  ASSERT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  ASSERT_TRUE(load_result.has_value());
  EXPECT_EQ(CrOSComponentManager::Error::NOT_FOUND, load_result.value());
  EXPECT_TRUE(mount_path.empty());

  EXPECT_TRUE(
      cros_component_manager.GetCompatiblePath(kTestComponentName).empty());

  EXPECT_FALSE(image_loader_client()->IsLoaded(kTestComponentName));
}

TEST_F(CrOSComponentInstallerTest, LoadNonInstalledComponent_DontForce_Mount) {
  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  base::Optional<base::FilePath> unpacked_path = CreateUnpackedComponent(
      kTestComponentName, "2.0", kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(unpacked_path.has_value());
  ASSERT_TRUE(updater.FinishForegroundUpdate(
      kTestComponentName, update_client::Error::NONE, unpacked_path.value()));
  RunUntilIdle();

  EXPECT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  VerifyComponentLoaded(cros_component_manager, kTestComponentName, load_result,
                        GetInstalledComponentPath(kTestComponentName, "2.0"));
  EXPECT_EQ(base::FilePath(kTestComponentMountPath), mount_path);
}

TEST_F(CrOSComponentInstallerTest,
       LoadComponentWithInstallFail_DontForce_Mount) {
  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  ASSERT_TRUE(updater.FinishForegroundUpdate(
      kTestComponentName, update_client::Error::SERVICE_ERROR,
      base::FilePath()));
  RunUntilIdle();

  EXPECT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  ASSERT_TRUE(load_result.has_value());
  EXPECT_EQ(CrOSComponentManager::Error::INSTALL_FAILURE, load_result.value());
  EXPECT_TRUE(mount_path.empty());

  EXPECT_TRUE(
      cros_component_manager.GetCompatiblePath(kTestComponentName).empty());

  EXPECT_FALSE(image_loader_client()->IsLoaded(kTestComponentName));
}

TEST_F(CrOSComponentInstallerTest,
       LoadWithObsoleteInstalledComponent_DontForce_Mount) {
  base::Optional<base::FilePath> old_install_path = CreateInstalledComponent(
      kTestComponentName, "0.5", kTestComponentInvalidMinEnvVersion);
  ASSERT_TRUE(old_install_path.has_value());
  base::Optional<base::FilePath> old_preinstall_path =
      CreatePreinstalledComponent(kTestComponentName, "0.5",
                                  kTestComponentInvalidMinEnvVersion);
  ASSERT_TRUE(old_preinstall_path.has_value());

  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  base::Optional<base::FilePath> unpacked_path = CreateUnpackedComponent(
      kTestComponentName, "2.0", kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(unpacked_path.has_value());
  ASSERT_TRUE(updater.FinishForegroundUpdate(
      kTestComponentName, update_client::Error::NONE, unpacked_path.value()));
  RunUntilIdle();

  EXPECT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  VerifyComponentLoaded(cros_component_manager, kTestComponentName, load_result,
                        GetInstalledComponentPath(kTestComponentName, "2.0"));
  EXPECT_EQ(base::FilePath(kTestComponentMountPath), mount_path);
}

TEST_F(CrOSComponentInstallerTest, RegisterAllRegistersInstalledComponent) {
  base::Optional<base::FilePath> install_path = CreateInstalledComponent(
      kTestComponentName, "1.0", kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(install_path.has_value());

  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  cros_component_manager.RegisterInstalled();
  RunUntilIdle();
  EXPECT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  EXPECT_EQ(install_path,
            cros_component_manager.GetCompatiblePath(kTestComponentName));
  EXPECT_FALSE(image_loader_client()->IsLoaded(kTestComponentName));
}

TEST_F(CrOSComponentInstallerTest, RegisterAllIgnoresPrenstalledComponent) {
  base::Optional<base::FilePath> preinstall_path = CreatePreinstalledComponent(
      kTestComponentName, "1.0", kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(preinstall_path.has_value());

  auto update_service = std::make_unique<MockComponentUpdateService>();
  EXPECT_CALL(*update_service, RegisterComponent(testing::_)).Times(0);
  EXPECT_CALL(*update_service, GetOnDemandUpdater()).Times(0);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  cros_component_manager.RegisterInstalled();
  RunUntilIdle();
  EXPECT_TRUE(
      cros_component_manager.GetCompatiblePath(kTestComponentName).empty());
  EXPECT_FALSE(image_loader_client()->IsLoaded(kTestComponentName));
}

TEST_F(CrOSComponentInstallerTest,
       LoadInstalledComponentAfterRegisterInstalled) {
  base::Optional<base::FilePath> install_path = CreateInstalledComponent(
      kTestComponentName, "1.0", kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(install_path.has_value());

  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  std::unique_ptr<MockComponentUpdateService> update_service =
      CreateUpdateServiceForSingleRegistration(kTestComponentName, &updater);
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  cros_component_manager.RegisterInstalled();
  RunUntilIdle();
  EXPECT_FALSE(updater.HasPendingUpdate(kTestComponentName));
  EXPECT_EQ(install_path.value(),
            cros_component_manager.GetCompatiblePath(kTestComponentName));

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  EXPECT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  VerifyComponentLoaded(cros_component_manager, kTestComponentName, load_result,
                        install_path.value());
  EXPECT_EQ(base::FilePath(kTestComponentMountPath), mount_path);
}

TEST_F(CrOSComponentInstallerTest,
       LoadInstalledComponentConcurrentWithRegisterInstalled) {
  base::Optional<base::FilePath> install_path = CreateInstalledComponent(
      kTestComponentName, "1.0", kTestComponentValidMinEnvVersion);
  ASSERT_TRUE(install_path.has_value());

  image_loader_client()->SetMountPathForComponent(
      kTestComponentName, base::FilePath(kTestComponentMountPath));

  TestUpdater updater;
  auto update_service = std::make_unique<MockComponentUpdateService>();
  EXPECT_CALL(*update_service,
              RegisterComponent(CrxComponentWithName(kTestComponentName)))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke(&updater, &TestUpdater::RegisterComponent));

  EXPECT_CALL(*update_service, GetOnDemandUpdater())
      .WillRepeatedly(testing::ReturnRef(updater));
  component_updater::CrOSComponentInstaller cros_component_manager(
      nullptr, update_service.get());

  cros_component_manager.RegisterInstalled();
  EXPECT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  base::Optional<CrOSComponentManager::Error> load_result;
  base::FilePath mount_path;
  cros_component_manager.Load(
      kTestComponentName, CrOSComponentManager::MountPolicy::kMount,
      CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&RecordLoadResult, &load_result, &mount_path));
  RunUntilIdle();

  EXPECT_FALSE(updater.HasPendingUpdate(kTestComponentName));

  VerifyComponentLoaded(cros_component_manager, kTestComponentName, load_result,
                        install_path.value());
  EXPECT_EQ(base::FilePath(kTestComponentMountPath), mount_path);
}

}  // namespace component_updater
