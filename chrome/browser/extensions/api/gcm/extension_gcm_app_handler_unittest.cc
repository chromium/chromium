// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_gcm_app_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/gcm/gcm_api.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/fake_gcm_app_handler.h"
#include "components/gcm_driver/fake_gcm_client.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/verifier_formats.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif

namespace extensions {

namespace {

const char kTestExtensionName[] = "FooBar";

void RequestProxyResolvingSocketFactoryOnUIThread(
    Profile* profile,
    base::WeakPtr<gcm::GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  if (!service)
    return;
  network::mojom::NetworkContext* network_context =
      profile->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->CreateProxyResolvingSocketFactory(std::move(receiver));
}

void RequestProxyResolvingSocketFactory(
    Profile* profile,
    base::WeakPtr<gcm::GCMProfileService> service,
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RequestProxyResolvingSocketFactoryOnUIThread,
                                profile, service, std::move(receiver)));
}

}  // namespace

// Helper class for asynchronous waiting.
class Waiter {
 public:
  Waiter() {}

  Waiter(const Waiter&) = delete;
  Waiter& operator=(const Waiter&) = delete;

  ~Waiter() {}

  // Waits until the asynchronous operation finishes.
  void WaitUntilCompleted() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // Signals that the asynchronous operation finishes.
  void SignalCompleted() {
    if (run_loop_ && run_loop_->running())
      run_loop_->Quit();
  }

  // Runs until UI loop becomes idle.
  void PumpUILoop() { base::RunLoop().RunUntilIdle(); }

  // Runs until IO loop becomes idle.
  void PumpIOLoop() {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&Waiter::OnIOLoopPump, base::Unretained(this)));

    WaitUntilCompleted();
  }

 private:
  void PumpIOLoopCompleted() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    SignalCompleted();
  }

  void OnIOLoopPump() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&Waiter::OnIOLoopPumpCompleted, base::Unretained(this)));
  }

  void OnIOLoopPumpCompleted() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&Waiter::PumpIOLoopCompleted, base::Unretained(this)));
  }

  std::unique_ptr<base::RunLoop> run_loop_;
};

class FakeExtensionGCMAppHandler : public ExtensionGCMAppHandler {
 public:
  FakeExtensionGCMAppHandler(Profile* profile, Waiter* waiter)
      : ExtensionGCMAppHandler(profile),
        waiter_(waiter),
        unregistration_result_(gcm::GCMClient::UNKNOWN_ERROR),
        delete_id_result_(instance_id::InstanceID::UNKNOWN_ERROR),
        app_handler_count_drop_to_zero_(false) {
  }

  FakeExtensionGCMAppHandler(const FakeExtensionGCMAppHandler&) = delete;
  FakeExtensionGCMAppHandler& operator=(const FakeExtensionGCMAppHandler&) =
      delete;

  ~FakeExtensionGCMAppHandler() override {}

  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override {}

  void OnMessagesDeleted(const std::string& app_id) override {}

  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override {}

  void OnUnregisterCompleted(const std::string& app_id,
                             gcm::GCMClient::Result result) override {
    ExtensionGCMAppHandler::OnUnregisterCompleted(app_id, result);
    unregistration_result_ = result;
    waiter_->SignalCompleted();
  }

  void OnDeleteIDCompleted(const std::string& app_id,
                           instance_id::InstanceID::Result result) override {
    delete_id_result_ = result;
    ExtensionGCMAppHandler::OnDeleteIDCompleted(app_id, result);
  }

  void RemoveAppHandler(const std::string& app_id) override {
    ExtensionGCMAppHandler::RemoveAppHandler(app_id);
    if (GetGCMDriver()->app_handlers().empty())
      app_handler_count_drop_to_zero_ = true;
  }

  gcm::GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }
  instance_id::InstanceID::Result delete_id_result() const {
    return delete_id_result_;
  }
  bool app_handler_count_drop_to_zero() const {
    return app_handler_count_drop_to_zero_;
  }

 private:
  raw_ptr<Waiter> waiter_;
  gcm::GCMClient::Result unregistration_result_;
  instance_id::InstanceID::Result delete_id_result_;
  bool app_handler_count_drop_to_zero_;
};

class ExtensionGCMAppHandlerTest : public testing::Test {
 public:
  static std::unique_ptr<KeyedService> BuildGCMProfileService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    scoped_refptr<base::SequencedTaskRunner> ui_thread =
        content::GetUIThreadTaskRunner({});
    scoped_refptr<base::SequencedTaskRunner> io_thread =
        content::GetIOThreadTaskRunner({});
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
    return std::make_unique<gcm::GCMProfileService>(
        profile->GetPrefs(), profile->GetPath(),
        base::BindRepeating(&RequestProxyResolvingSocketFactory, profile),
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess(),
        network::TestNetworkConnectionTracker::GetInstance(),
        chrome::GetChannel(),
        gcm::GetProductCategoryForSubtypes(profile->GetPrefs()),
        IdentityManagerFactory::GetForProfile(profile),
        base::WrapUnique(new gcm::FakeGCMClientFactory(ui_thread, io_thread)),
        ui_thread, io_thread, blocking_task_runner);
  }

  ExtensionGCMAppHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        extension_service_(nullptr),
        registration_result_(gcm::GCMClient::UNKNOWN_ERROR),
        unregistration_result_(gcm::GCMClient::UNKNOWN_ERROR) {}

  ExtensionGCMAppHandlerTest(const ExtensionGCMAppHandlerTest&) = delete;
  ExtensionGCMAppHandlerTest& operator=(const ExtensionGCMAppHandlerTest&) =
      delete;

  ~ExtensionGCMAppHandlerTest() override {}

  // Overridden from test::Test:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Allow extension update to unpack crx in process.
    in_process_utility_thread_helper_ =
        std::make_unique<content::InProcessUtilityThreadHelper>();

    // This is needed to create extension service under CrOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<ash::UserManagerDelegateImpl>(),
        g_browser_process->local_state(), ash::CrosSettings::Get()));
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
#endif

    // Create a new profile.
    TestingProfile::Builder builder;
    profile_ = builder.Build();

    // Create extension service in order to uninstall the extension.
    TestExtensionSystem* extension_system(
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile())));
    base::FilePath extensions_install_dir =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Extensions"));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), extensions_install_dir, false);
    extension_service_ = extension_system->Get(profile())->extension_service();

    // Create GCMProfileService that talks with fake GCMClient.
    gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(
                       &ExtensionGCMAppHandlerTest::BuildGCMProfileService));

    // Create a fake version of ExtensionGCMAppHandler.
    gcm_app_handler_ =
        std::make_unique<FakeExtensionGCMAppHandler>(profile(), &waiter_);
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_.Reset();
#endif

    waiter_.PumpUILoop();
    gcm_app_handler_->Shutdown();
    auto* partition = profile()->GetDefaultStoragePartition();
    if (partition)
      partition->WaitForDeletionTasksForTesting();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    gcm_app_handler_.reset();
    profile_.reset();
    ash::ConciergeClient::Shutdown();
#endif
  }

  // Returns a barebones test extension.
  scoped_refptr<const Extension> CreateExtension() {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(kTestExtensionName)
            .AddAPIPermission("gcm")
            .SetPath(temp_dir_.GetPath())
            .SetID("ldnnhddmnhbkjipkidpdiheffobcpfmf")
            .Build();
    EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
        mojom::APIPermissionID::kGcm));

    return extension;
  }

  void LoadExtension(const Extension* extension) {
    extension_service_->AddExtension(extension);
  }

  void InstallerDone(const std::optional<CrxInstallError>& error) {
    ASSERT_FALSE(error);
    waiter_.SignalCompleted();
  }

  void UpdateExtension(const Extension* extension,
                       const std::string& update_crx) {
    base::FilePath data_dir;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &data_dir)) {
      ADD_FAILURE();
      return;
    }
    data_dir = data_dir.AppendASCII("extensions");
    data_dir = data_dir.AppendASCII(update_crx);

    base::FilePath path = temp_dir_.GetPath();
    path = path.Append(data_dir.BaseName());
    ASSERT_TRUE(base::CopyFile(data_dir, path));

    extensions::CRXFileInfo crx_info(path, extensions::GetTestVerifierFormat());
    crx_info.extension_id = extension->id();

    auto installer = extension_service_->CreateUpdateInstaller(crx_info, true);
    installer->AddInstallerCallback(base::BindOnce(
        &ExtensionGCMAppHandlerTest::InstallerDone, base::Unretained(this)));
    installer->InstallCrxFile(crx_info);

    waiter_.WaitUntilCompleted();
  }

  void DisableExtension(const Extension* extension) {
    extension_service_->DisableExtension(extension->id(),
                                         disable_reason::DISABLE_USER_ACTION);
  }

  void EnableExtension(const Extension* extension) {
    extension_service_->EnableExtension(extension->id());
  }

  void UninstallExtension(const Extension* extension) {
    extension_service_->UninstallExtension(
        extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  }

  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids) {
    GetGCMDriver()->Register(
        app_id, sender_ids,
        base::BindOnce(&ExtensionGCMAppHandlerTest::RegisterCompleted,
                       base::Unretained(this)));
  }

  void RegisterCompleted(const std::string& registration_id,
                         gcm::GCMClient::Result result) {
    registration_result_ = result;
    waiter_.SignalCompleted();
  }

  gcm::GCMDriver* GetGCMDriver() const {
    return gcm::GCMProfileServiceFactory::GetForProfile(profile())->driver();
  }

  bool HasAppHandlers(const std::string& app_id) const {
    return GetGCMDriver()->app_handlers().count(app_id);
  }

  Profile* profile() const { return profile_.get(); }
  Waiter* waiter() { return &waiter_; }
  FakeExtensionGCMAppHandler* gcm_app_handler() const {
    return gcm_app_handler_.get();
  }
  gcm::GCMClient::Result registration_result() const {
    return registration_result_;
  }
  gcm::GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::InProcessUtilityThreadHelper>
      in_process_utility_thread_helper_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ExtensionService, DanglingUntriaged>
      extension_service_;  // Not owned.
  base::ScopedTempDir temp_dir_;

  // This is needed to create extension service under CrOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_;
#endif

  Waiter waiter_;
  std::unique_ptr<FakeExtensionGCMAppHandler> gcm_app_handler_;
  gcm::GCMClient::Result registration_result_;
  gcm::GCMClient::Result unregistration_result_;
};

TEST_F(ExtensionGCMAppHandlerTest, AddAndRemoveAppHandler) {
  scoped_refptr<const Extension> extension(CreateExtension());

  // App handler is added when extension is loaded.
  LoadExtension(extension.get());
  waiter()->PumpUILoop();
  EXPECT_TRUE(HasAppHandlers(extension->id()));

  // App handler is removed when extension is unloaded.
  DisableExtension(extension.get());
  waiter()->PumpUILoop();
  EXPECT_FALSE(HasAppHandlers(extension->id()));

  // App handler is added when extension is reloaded.
  EnableExtension(extension.get());
  waiter()->PumpUILoop();
  EXPECT_TRUE(HasAppHandlers(extension->id()));

  // App handler is removed when extension is uninstalled.
  UninstallExtension(extension.get());
  waiter()->WaitUntilCompleted();
  EXPECT_FALSE(HasAppHandlers(extension->id()));
}

TEST_F(ExtensionGCMAppHandlerTest, UnregisterOnExtensionUninstall) {
  scoped_refptr<const Extension> extension(CreateExtension());
  LoadExtension(extension.get());

  // Kick off registration.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(extension->id(), sender_ids);
  waiter()->WaitUntilCompleted();
  EXPECT_EQ(gcm::GCMClient::SUCCESS, registration_result());

  // Both token deletion and unregistration should be triggered when the
  // extension is uninstalled.
  UninstallExtension(extension.get());
  waiter()->WaitUntilCompleted();
  EXPECT_EQ(instance_id::InstanceID::SUCCESS,
            gcm_app_handler()->delete_id_result());
  EXPECT_EQ(gcm::GCMClient::SUCCESS,
            gcm_app_handler()->unregistration_result());
}

TEST_F(ExtensionGCMAppHandlerTest, UpdateExtensionWithGcmPermissionKept) {
  scoped_refptr<const Extension> extension(CreateExtension());

  // App handler is added when the extension is loaded.
  LoadExtension(extension.get());
  waiter()->PumpUILoop();
  EXPECT_TRUE(HasAppHandlers(extension->id()));

  // App handler count should not drop to zero when the extension is updated.
  UpdateExtension(extension.get(), "gcm2.crx");
  waiter()->PumpUILoop();
  EXPECT_FALSE(gcm_app_handler()->app_handler_count_drop_to_zero());
  EXPECT_TRUE(HasAppHandlers(extension->id()));
}

TEST_F(ExtensionGCMAppHandlerTest, UpdateExtensionWithGcmPermissionRemoved) {
  scoped_refptr<const Extension> extension(CreateExtension());

  // App handler is added when the extension is loaded.
  LoadExtension(extension.get());
  waiter()->PumpUILoop();
  EXPECT_TRUE(HasAppHandlers(extension->id()));

  // App handler is removed when the extension is updated to the version that
  // has GCM permission removed.
  UpdateExtension(extension.get(), "good2.crx");
  waiter()->PumpUILoop();
  EXPECT_TRUE(gcm_app_handler()->app_handler_count_drop_to_zero());
  EXPECT_FALSE(HasAppHandlers(extension->id()));
}

}  // namespace extensions
