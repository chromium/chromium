// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_package_service.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace crostini {

namespace {

using ::chromeos::DBusMethodCallback;
using ::chromeos::DBusThreadManager;
using ::chromeos::FakeCiceroneClient;
using ::chromeos::FakeSeneschalClient;
using ::testing::_;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::UnorderedElementsAre;
using ::vm_tools::cicerone::InstallLinuxPackageProgressSignal;
using ::vm_tools::cicerone::InstallLinuxPackageRequest;
using ::vm_tools::cicerone::InstallLinuxPackageResponse;
using ::vm_tools::cicerone::LinuxPackageInfoRequest;
using ::vm_tools::cicerone::LinuxPackageInfoResponse;
using ::vm_tools::cicerone::PendingAppListUpdatesSignal;
using ::vm_tools::cicerone::UninstallPackageOwningFileRequest;
using ::vm_tools::cicerone::UninstallPackageOwningFileResponse;
using ::vm_tools::cicerone::UninstallPackageProgressSignal;
using ::vm_tools::seneschal::SharePathResponse;

// IDs, etc of apps that are always registered during tests.
// These are on the default VM / default container.
constexpr char kDefaultAppFileId[] = "default_file_id";
constexpr char kDefaultAppName[] = "The Default";
constexpr char kSecondAppFileId[] = "second_file_id";
constexpr char kSecondAppName[] = "Another Fine App";
constexpr char kThirdAppFileId[] = "third_file_id";
constexpr char kThirdAppName[] = "Yet Another App";
// Different VM name, but container name is the default.
constexpr char kDifferentVmAppFileId[] = "different_vm_app";
constexpr char kDifferentVmAppName[] = "I'm in a VM!";
constexpr char kDifferentVmApp2FileId[] = "different_vm_app_2";
constexpr char kDifferentVmApp2Name[] = "I'm in a VM also";
constexpr char kDifferentVmVmName[] = "second_vm_name";
// Default VM name, but container name is different.
constexpr char kDifferentContainerAppFileId[] = "different_container_app";
constexpr char kDifferentContainerAppName[] =
    "Just Over The Container Boundary";
constexpr char kDifferentContainerApp2FileId[] = "different_container_app_2";
constexpr char kDifferentContainerApp2Name[] = "Severe Lack of Containers";
constexpr char kDifferentContainerContainerName[] = "second_container_name";
constexpr char kPackageFilePath[] = "/tmp/nethack.deb";
constexpr char kPackageFileContainerPath[] =
    "/mnt/chromeos/MyFiles/tmp/nethack.deb";

// Callback for RunUntilUninstallRequestMade.
void CaptureUninstallRequestParametersAndQuitLoop(
    base::RepeatingClosure quit_closure,
    UninstallPackageOwningFileRequest* request_output,
    DBusMethodCallback<UninstallPackageOwningFileResponse>* callback_output,
    const UninstallPackageOwningFileRequest& request_input,
    DBusMethodCallback<UninstallPackageOwningFileResponse> callback_input) {
  *request_output = request_input;
  if (callback_output != nullptr) {
    *callback_output = std::move(callback_input);
  }
  std::move(quit_closure).Run();
}

// Run until |fake_cicerone_client_|'s UninstallPackageOwningFile is called.
// |request| and |callback| are filled in with the parameters to
// CiceroneClient::UninstallPackageOwningFile.
void RunUntilUninstallRequestMade(
    FakeCiceroneClient* fake_cicerone_client,
    UninstallPackageOwningFileRequest* request,
    DBusMethodCallback<UninstallPackageOwningFileResponse>* callback) {
  base::RunLoop run_loop;
  fake_cicerone_client->SetOnUninstallPackageOwningFileCallback(
      base::BindRepeating(&CaptureUninstallRequestParametersAndQuitLoop,
                          run_loop.QuitClosure(), base::Unretained(request),
                          base::Unretained(callback)));
  run_loop.Run();

  // Callback isn't valid after end of function.
  fake_cicerone_client->SetOnUninstallPackageOwningFileCallback(
      base::NullCallback());
}

// Callback used for InstallLinuxPackage
void ExpectedCrostiniResult(base::OnceClosure quit,
                            CrostiniResult expected,
                            CrostiniResult result) {
  EXPECT_EQ(expected, result);
  std::move(quit).Run();
}

// Callback used for GetLinuxPackageInfo.
void RecordPackageInfoResult(LinuxPackageInfo* record_location,
                             const LinuxPackageInfo& result) {
  *record_location = result;
}

class CrostiniPackageServiceTest : public testing::Test {
 public:
  CrostiniPackageServiceTest()
      : kDefaultAppId(CrostiniTestHelper::GenerateAppId(kDefaultAppFileId)),
        kSecondAppId(CrostiniTestHelper::GenerateAppId(kSecondAppFileId)),
        kThirdAppId(CrostiniTestHelper::GenerateAppId(kThirdAppFileId)),
        kDifferentVmAppId(
            CrostiniTestHelper::GenerateAppId(kDifferentVmAppFileId,
                                              kDifferentVmVmName,
                                              kCrostiniDefaultContainerName)),
        kDifferentVmApp2Id(
            CrostiniTestHelper::GenerateAppId(kDifferentVmApp2FileId,
                                              kDifferentVmVmName,
                                              kCrostiniDefaultContainerName)),
        kDifferentContainerAppId(CrostiniTestHelper::GenerateAppId(
            kDifferentContainerAppFileId,
            kCrostiniDefaultVmName,
            kDifferentContainerContainerName)),
        kDifferentContainerApp2Id(CrostiniTestHelper::GenerateAppId(
            kDifferentContainerApp2FileId,
            kCrostiniDefaultVmName,
            kDifferentContainerContainerName)) {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    fake_cicerone_client_ = static_cast<FakeCiceroneClient*>(
        DBusThreadManager::Get()->GetCiceroneClient());
    ASSERT_TRUE(fake_cicerone_client_);
    fake_seneschal_client_ = static_cast<FakeSeneschalClient*>(
        DBusThreadManager::Get()->GetSeneschalClient());
    ASSERT_TRUE(fake_seneschal_client_);

    task_environment_ = std::make_unique<content::BrowserTaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::UI,
        base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
        content::BrowserTaskEnvironment::REAL_IO_THREAD);
    profile_ = std::make_unique<TestingProfile>(
        base::FilePath("/home/chronos/u-0123456789abcdef"));
    crostini_test_helper_ =
        std::make_unique<CrostiniTestHelper>(profile_.get());
    notification_display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());
    notification_display_service_ =
        static_cast<StubNotificationDisplayService*>(
            NotificationDisplayServiceFactory::GetForProfile(profile_.get()));
    ASSERT_TRUE(notification_display_service_);
    service_ = std::make_unique<CrostiniPackageService>(profile_.get());
    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    ASSERT_TRUE(mount_points);
    std::string mount_point_name =
        file_manager::util::GetDownloadsMountPointName(profile_.get());
    mount_points->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeNativeLocal,
        storage::FileSystemMountOption(),
        file_manager::util::GetDownloadsFolderForProfile(profile_.get()));
    package_file_url_ = mount_points->CreateExternalFileSystemURL(
        GURL(), mount_point_name, base::FilePath(kPackageFilePath));

    auto* crostini_manager = CrostiniManager::GetForProfile(profile_.get());
    ASSERT_TRUE(crostini_manager);
    crostini_manager->set_skip_restart_for_testing();
    crostini_manager->AddRunningVmForTesting(kDifferentVmVmName);

    CreateDefaultAppRegistration();
    CreateSecondAppRegistration();
    CreateThirdAppRegistration();
    CreateDifferentVmAppRegistration();
    CreateDifferentContainerAppRegistration();
  }

  void TearDown() override {
    // Complete all CrostiniManager queued tasks before deleting it.
    base::RunLoop().RunUntilIdle();
    service_.reset();
    notification_display_service_tester_.reset();
    crostini_test_helper_.reset();
    profile_.reset();
    task_environment_.reset();
    DBusThreadManager::Shutdown();
  }

 protected:
  const std::string kDefaultAppId;  // App_id for app with kDefaultAppFileId.
  const std::string kSecondAppId;   // App_id for app with kSecondAppFileId.
  const std::string kThirdAppId;    // App_id for app with kThirdAppFileId.
  const std::string kDifferentVmAppId;          // App_id for app with
                                                // kDifferentVmAppFileId.
  const std::string kDifferentVmApp2Id;         // App_id for app with
                                                // kDifferentVmApp2FileId.
  const std::string kDifferentContainerAppId;   // App_id for app with
                                                // kDifferentContainerAppFileId.
  const std::string kDifferentContainerApp2Id;  // App_id for app with
                                                // kDifferentContainerApp2FileId
  storage::FileSystemURL package_file_url_;

  UninstallPackageProgressSignal MakeUninstallSignal(
      const UninstallPackageOwningFileRequest& request) {
    UninstallPackageProgressSignal signal;
    signal.set_vm_name(request.vm_name());
    signal.set_container_name(request.container_name());
    signal.set_owner_id(request.owner_id());
    return signal;
  }

  InstallLinuxPackageProgressSignal MakeInstallSignal(
      const InstallLinuxPackageRequest& request) {
    InstallLinuxPackageProgressSignal signal;
    signal.set_vm_name(request.vm_name());
    signal.set_container_name(request.container_name());
    signal.set_owner_id(request.owner_id());
    return signal;
  }

  void SendAppListUpdateSignal(const std::string& vm_name,
                               const std::string& container_name,
                               int count) {
    PendingAppListUpdatesSignal signal;
    signal.set_vm_name(vm_name);
    signal.set_container_name(container_name);
    signal.set_count(count);
    fake_cicerone_client_->NotifyPendingAppListUpdates(signal);
  }

  // Closes the notification as if the user had clicked 'close'.
  void CloseNotification(const message_center::Notification& notification) {
    notification_display_service_->RemoveNotification(
        NotificationHandler::Type::TRANSIENT, notification.id(),
        true /*by_user*/, false /*silent*/);
  }

  // Start an uninstall and then sent a single uninstall signal with the given
  // status and (optional) progress. If |request_out| is not nullptr, the
  // request sent by the service will be copied to |*request_out|.
  void StartAndSignalUninstall(
      UninstallPackageProgressSignal::Status signal_status,
      int progress_percent = 0,
      const char* const expected_desktop_file_id = kDefaultAppFileId,
      UninstallPackageOwningFileRequest* request_out = nullptr) {
    UninstallPackageOwningFileRequest request;
    DBusMethodCallback<UninstallPackageOwningFileResponse> callback;
    RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);
    EXPECT_EQ(request.desktop_file_id(), expected_desktop_file_id);

    // It's illegal to send a signal unless the response was STARTED. So assume
    // that any test calling this function wants us to give a STARTED response.
    UninstallPackageOwningFileResponse response;
    response.set_status(UninstallPackageOwningFileResponse::STARTED);
    std::move(callback).Run(response);

    UninstallPackageProgressSignal signal = MakeUninstallSignal(request);
    signal.set_status(signal_status);
    switch (signal_status) {
      case UninstallPackageProgressSignal::UNINSTALLING:
        signal.set_progress_percent(progress_percent);
        break;
      case UninstallPackageProgressSignal::FAILED:
        signal.set_failure_details("Oh no not again");
        break;
      case UninstallPackageProgressSignal::SUCCEEDED:
        break;
      default:
        NOTREACHED();
    }
    fake_cicerone_client_->UninstallPackageProgress(signal);

    if (request_out != nullptr) {
      *request_out = request;
    }
  }

  // Start an install and then sent a single install signal with the given
  // status and (optional) progress.
  void StartAndSignalInstall(
      InstallLinuxPackageProgressSignal::Status signal_status,
      int progress_percent = 0) {
    base::RunLoop().RunUntilIdle();

    InstallLinuxPackageProgressSignal signal = MakeInstallSignal(
        fake_cicerone_client_->get_most_recent_install_linux_package_request());
    signal.set_status(signal_status);
    switch (signal_status) {
      case InstallLinuxPackageProgressSignal::DOWNLOADING:
      case InstallLinuxPackageProgressSignal::INSTALLING:
        signal.set_progress_percent(progress_percent);
        break;

      case InstallLinuxPackageProgressSignal::FAILED:
        signal.set_failure_details("Wouldn't be prudent");
        break;

      case InstallLinuxPackageProgressSignal::SUCCEEDED:
        break;

      default:
        NOTREACHED();
    }
    fake_cicerone_client_->InstallLinuxPackageProgress(signal);
  }

  // Owned by DBusThreadManager
  FakeCiceroneClient* fake_cicerone_client_ = nullptr;
  FakeSeneschalClient* fake_seneschal_client_ = nullptr;

  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniTestHelper> crostini_test_helper_;
  std::unique_ptr<NotificationDisplayServiceTester>
      notification_display_service_tester_;
  StubNotificationDisplayService* notification_display_service_;
  std::unique_ptr<CrostiniPackageService> service_;

 private:
  // Helper to set up an App proto with a single name.
  vm_tools::apps::App BasicApp(const std::string& desktop_file_id,
                               const std::string& name,
                               const std::string& package_id) {
    vm_tools::apps::App app;
    app.set_desktop_file_id(desktop_file_id);
    app.mutable_name()->add_values()->set_value(name);
    app.set_no_display(false);
    app.set_package_id(package_id);
    return app;
  }

  // Create a registration in CrostiniRegistryService for an app with app_id
  // kDefaultAppId and desktop file ID kDefaultAppFileId.
  void CreateDefaultAppRegistration() {
    auto app = BasicApp(kDefaultAppFileId, kDefaultAppName, "123-thing");
    crostini_test_helper_->AddApp(app);
  }

  // Create a registration in CrostiniRegistryService for an app with app_id
  // kSecondAppId and desktop file ID kSecondAppFileId.
  void CreateSecondAppRegistration() {
    auto app = BasicApp(kSecondAppFileId, kSecondAppName, "abc-another");
    crostini_test_helper_->AddApp(app);
  }

  // Create a registration in CrostiniRegistryService for an app with app_id
  // kThirdAppId and desktop file ID kThirdAppFileId.
  void CreateThirdAppRegistration() {
    auto app = BasicApp(kThirdAppFileId, kThirdAppName, "yanpi");
    crostini_test_helper_->AddApp(app);
  }

  // Create a registration in CrostiniRegistryService for apps with app_id
  // kDifferentVmAppId and kDifferentVmApp2Id inside kDifferentVmVmName.
  void CreateDifferentVmAppRegistration() {
    // CrostiniTestHelper doesn't directly allow apps to be added for VMs other
    // than the default VM.
    vm_tools::apps::ApplicationList app_list;
    app_list.set_vm_name(kDifferentVmVmName);
    app_list.set_container_name(kCrostiniDefaultContainerName);
    *app_list.add_apps() =
        BasicApp(kDifferentVmAppFileId, kDifferentVmAppName, "pack5");
    *app_list.add_apps() =
        BasicApp(kDifferentVmApp2FileId, kDifferentVmApp2Name, "pack5-2");
    crostini::CrostiniRegistryServiceFactory::GetForProfile(profile_.get())
        ->UpdateApplicationList(app_list);
  }

  // Create a registration in CrostiniRegistryService for apps with app_id
  // kDifferentContainerAppId and kDifferentContainerApp2Id inside
  // kDifferentContainerContainerName.
  void CreateDifferentContainerAppRegistration() {
    // CrostiniTestHelper doesn't directly allow apps to be added for containers
    // other than the default container.
    vm_tools::apps::ApplicationList app_list;
    app_list.set_vm_name(kCrostiniDefaultVmName);
    app_list.set_container_name(kDifferentContainerContainerName);
    *app_list.add_apps() = BasicApp(kDifferentContainerAppFileId,
                                    kDifferentContainerAppName, "pack7");
    *app_list.add_apps() = BasicApp(kDifferentContainerApp2FileId,
                                    kDifferentContainerApp2Name, "pack7-2");
    crostini::CrostiniRegistryServiceFactory::GetForProfile(profile_.get())
        ->UpdateApplicationList(app_list);
  }
};

// A way of referring to one of the various app ids in parameters.
enum KnownApp {
  DEFAULT_APP,
  SECOND_APP,
  THIRD_APP,
  DIFFERENT_VM,
  DIFFERENT_VM_2,
  DIFFERENT_CONTAINER,
  DIFFERENT_CONTAINER_2
};

// Returns the app name for one of the known apps.
base::string16 GetAppName(KnownApp app) {
  switch (app) {
    case DEFAULT_APP:
      return base::ASCIIToUTF16(kDefaultAppName);
    case SECOND_APP:
      return base::ASCIIToUTF16(kSecondAppName);
    case THIRD_APP:
      return base::ASCIIToUTF16(kThirdAppName);
    case DIFFERENT_VM:
      return base::ASCIIToUTF16(kDifferentVmAppName);
    case DIFFERENT_VM_2:
      return base::ASCIIToUTF16(kDifferentVmApp2Name);
    case DIFFERENT_CONTAINER:
      return base::ASCIIToUTF16(kDifferentContainerAppName);
    case DIFFERENT_CONTAINER_2:
      return base::ASCIIToUTF16(kDifferentContainerApp2Name);
    default:
      NOTREACHED();
  }
}

// This is a wrapper around message_center::Notification, allowing us to print
// them nicely. Otherwise, UnorderedElementsAre has failures that look like
// Actual: { 680-byte object <68-98 31-13 AC-7F 00-00 00-00 00-00 AB-AB AB-AB
//           80-AC 97-F1 06-23 00-00 1C-00 00-00 00-00 00-00 20-00 00-00 00-00
//           00-80 80-49 9F-F1 06-23 00-00 0B-00 00-00 00-00 00-00 10-00 00-00
//           00-00 00-80 ... 00-00 00-00 00-00 00-00 D2-67 19-FF 00-00 00-00
//           00-75 59-F1 00-00 00-00 03-00 00-00 AB-AB AB-AB E0-40 92-F1 06-23
//           00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00
//           00-00 00-00> }
class PrintableNotification {
 public:
  explicit PrintableNotification(const message_center::Notification& base)
      : display_source_(base.display_source()),
        title_(base.title()),
        message_(base.message()),
        progress_(base.progress()) {}

  const base::string16& display_source() const { return display_source_; }
  const base::string16& title() const { return title_; }
  const base::string16& message() const { return message_; }
  int progress() const { return progress_; }

 private:
  const base::string16 display_source_;
  const base::string16 title_;
  const base::string16 message_;
  const int progress_;
};

std::ostream& operator<<(std::ostream& os,
                         const PrintableNotification& notification) {
  os << "source: \"" << notification.display_source() << "\", title: \""
     << notification.title() << "\", message: \"" << notification.message()
     << "\", progress: " << notification.progress();
  return os;
}

// Short-named conversion functions to avoid adding more noise than necessary
// to EXPECT_THAT calls.
PrintableNotification Printable(const message_center::Notification& base) {
  return PrintableNotification(base);
}

std::vector<PrintableNotification> Printable(
    const std::vector<message_center::Notification>& base) {
  std::vector<PrintableNotification> result;
  result.reserve(base.size());
  for (const message_center::Notification& base_notification : base) {
    result.push_back(PrintableNotification(base_notification));
  }
  return result;
}

class NotificationMatcher : public MatcherInterface<PrintableNotification> {
 public:
  NotificationMatcher(const base::string16& expected_source,
                      const base::string16& expected_title,
                      const base::string16& expected_message)
      : expected_source_(expected_source),
        expected_title_(expected_title),
        check_message_(true),
        expected_message_(expected_message),
        check_progress_(false),
        expected_progress_(-1) {}
  NotificationMatcher(const base::string16& expected_source,
                      const base::string16& expected_title,
                      int expected_progress)
      : expected_source_(expected_source),
        expected_title_(expected_title),
        check_message_(false),
        expected_message_(),
        check_progress_(true),
        expected_progress_(expected_progress) {}

  bool MatchAndExplain(PrintableNotification notification,
                       MatchResultListener* listener) const override {
    bool has_mismatch = false;
    if (notification.display_source() != expected_source_) {
      *listener << "notification source: " << notification.display_source()
                << "\ndoes not equal expected source: " << expected_source_;
      has_mismatch = true;
    }
    if (notification.title() != expected_title_) {
      if (has_mismatch) {
        *listener << "\nand\n";
      }
      *listener << "notification title: " << notification.title()
                << "\ndoes not equal expected title: " << expected_title_;
      has_mismatch = true;
    }
    if (check_message_ && (notification.message() != expected_message_)) {
      if (has_mismatch) {
        *listener << "\nand\n";
      }
      *listener << "notification message: " << notification.message()
                << "\ndoes not equal expected message: " << expected_message_;
      has_mismatch = true;
    }
    if (check_progress_ && (notification.progress() != expected_progress_)) {
      if (has_mismatch) {
        *listener << "\nand\n";
      }
      *listener << "notification progress: " << notification.progress()
                << "\ndoes not equal expected progress: " << expected_progress_;
      has_mismatch = true;
    }
    return !has_mismatch;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "has notification source \"" << expected_source_ << "\" and title \""
        << expected_title_ << "\"";
    if (check_message_) {
      *os << " and message \"" << expected_message_ << "\"";
    }
    if (check_progress_) {
      *os << " and progress " << expected_progress_;
    }
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "does not have notification source \"" << expected_source_
        << "\" or does not have title \"" << expected_title_ << "\"";
    if (check_message_) {
      *os << " or does not have message \"" << expected_message_ << "\"";
    }
    if (check_progress_) {
      *os << " or does not have progress " << expected_progress_;
    }
  }

 private:
  const base::string16 expected_source_;
  const base::string16 expected_title_;
  const bool check_message_;
  const base::string16 expected_message_;
  const bool check_progress_;
  const int expected_progress_;
};

Matcher<PrintableNotification> IsUninstallSuccessNotification(
    KnownApp app = DEFAULT_APP) {
  return MakeMatcher(new NotificationMatcher(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_DISPLAY_SOURCE),
      l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_COMPLETED_TITLE,
          GetAppName(app)),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_COMPLETED_MESSAGE)));
}

Matcher<PrintableNotification> IsUninstallFailedNotification(
    KnownApp app = DEFAULT_APP) {
  return MakeMatcher(new NotificationMatcher(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_DISPLAY_SOURCE),
      l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_ERROR_TITLE,
          GetAppName(app)),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_ERROR_MESSAGE)));
}

Matcher<PrintableNotification> IsUninstallProgressNotification(
    int expected_progress,
    KnownApp app = DEFAULT_APP) {
  return MakeMatcher(new NotificationMatcher(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_DISPLAY_SOURCE),
      l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_IN_PROGRESS_TITLE,
          GetAppName(app)),
      expected_progress));
}

Matcher<PrintableNotification> IsUninstallWaitingForAppListNotification(
    KnownApp app = DEFAULT_APP) {
  return MakeMatcher(new NotificationMatcher(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_DISPLAY_SOURCE),
      l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_IN_PROGRESS_TITLE,
          GetAppName(app)),
      -1));
}

Matcher<PrintableNotification> IsUninstallQueuedNotification(
    KnownApp app = DEFAULT_APP) {
  return MakeMatcher(new NotificationMatcher(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_DISPLAY_SOURCE),
      l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_QUEUED_TITLE,
          GetAppName(app)),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_QUEUED_MESSAGE)));
}

Matcher<PrintableNotification> IsInstallProgressNotification(
    int expected_progress) {
  return MakeMatcher(new NotificationMatcher(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_DISPLAY_SOURCE),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_IN_PROGRESS_TITLE),
      expected_progress));
}

Matcher<PrintableNotification> IsInstallWaitingForAppListNotification() {
  return MakeMatcher(new NotificationMatcher(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_DISPLAY_SOURCE),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_IN_PROGRESS_TITLE),
      -1));
}

Matcher<PrintableNotification> IsInstallSuccessNotification() {
  return MakeMatcher(new NotificationMatcher(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_DISPLAY_SOURCE),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_TITLE),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_MESSAGE)));
}

Matcher<PrintableNotification> IsInstallFailedNotification() {
  return MakeMatcher(new NotificationMatcher(
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_DISPLAY_SOURCE),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_TITLE),
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_MESSAGE)));
}

TEST_F(CrostiniPackageServiceTest, BasicUninstallMakesValidUninstallRequest) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, nullptr);

  EXPECT_EQ(request.vm_name(), kCrostiniDefaultVmName);
  EXPECT_EQ(request.container_name(), kCrostiniDefaultContainerName);
  EXPECT_EQ(request.owner_id(), CryptohomeIdForProfile(profile_.get()));
  EXPECT_EQ(request.desktop_file_id(), kDefaultAppFileId);
}

TEST_F(CrostiniPackageServiceTest, DifferentVmMakesValidUninstallRequest) {
  service_->QueueUninstallApplication(kDifferentVmAppId);

  UninstallPackageOwningFileRequest request;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, nullptr);

  EXPECT_EQ(request.vm_name(), kDifferentVmVmName);
  EXPECT_EQ(request.container_name(), kCrostiniDefaultContainerName);
  EXPECT_EQ(request.owner_id(), CryptohomeIdForProfile(profile_.get()));
  EXPECT_EQ(request.desktop_file_id(), kDifferentVmAppFileId);
}

TEST_F(CrostiniPackageServiceTest,
       DifferentContainerMakesValidUninstallRequest) {
  service_->QueueUninstallApplication(kDifferentContainerAppId);

  UninstallPackageOwningFileRequest request;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, nullptr);

  EXPECT_EQ(request.vm_name(), kCrostiniDefaultVmName);
  EXPECT_EQ(request.container_name(), kDifferentContainerContainerName);
  EXPECT_EQ(request.owner_id(), CryptohomeIdForProfile(profile_.get()));
  EXPECT_EQ(request.desktop_file_id(), kDifferentContainerAppFileId);
}

TEST_F(CrostiniPackageServiceTest, BasicUninstallDisplaysNotification) {
  service_->QueueUninstallApplication(kDefaultAppId);
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       FailedUninstallResponseDisplaysFailedNotification) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);

  // Tell service the uninstall failed.
  UninstallPackageOwningFileResponse response;
  response.set_status(UninstallPackageOwningFileResponse::FAILED);
  response.set_failure_reason("I prefer not to");
  std::move(callback).Run(response);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallFailedNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       BlockedUninstallResponseDisplaysFailedNotification) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);

  // Tell service the uninstall failed.
  UninstallPackageOwningFileResponse response;
  response.set_status(
      UninstallPackageOwningFileResponse::BLOCKING_OPERATION_IN_PROGRESS);
  response.set_failure_reason("Hahaha NO");
  std::move(callback).Run(response);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallFailedNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       FailedUninstallSignalDisplaysFailedNotification) {
  service_->QueueUninstallApplication(kDefaultAppId);

  StartAndSignalUninstall(UninstallPackageProgressSignal::FAILED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallFailedNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallDisplaysProgressNotificationBeforeResponse) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, nullptr);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(0)));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallDisplaysProgressNotificationBeforeSignal) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);
  UninstallPackageOwningFileResponse response;
  response.set_status(UninstallPackageOwningFileResponse::STARTED);
  std::move(callback).Run(response);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(0)));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallDisplaysProgressNotificationAfterProgressSignal) {
  service_->QueueUninstallApplication(kDefaultAppId);

  StartAndSignalUninstall(UninstallPackageProgressSignal::UNINSTALLING,
                          23 /*progress_percent*/);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(23)));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallDisplaysSuccessNotificationAfterProgressThenSuccess) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  StartAndSignalUninstall(UninstallPackageProgressSignal::UNINSTALLING,
                          50 /*progress_percent*/, kDefaultAppFileId, &request);

  UninstallPackageProgressSignal signal_success = MakeUninstallSignal(request);
  signal_success.set_status(UninstallPackageProgressSignal::SUCCEEDED);
  fake_cicerone_client_->UninstallPackageProgress(signal_success);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallDisplaysFailureNotificationAfterProgressThenFailure) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  StartAndSignalUninstall(UninstallPackageProgressSignal::UNINSTALLING,
                          50 /*progress_percent*/, kDefaultAppFileId, &request);

  UninstallPackageProgressSignal signal_failure = MakeUninstallSignal(request);
  signal_failure.set_status(UninstallPackageProgressSignal::FAILED);
  signal_failure.set_failure_details("I prefer not to");
  fake_cicerone_client_->UninstallPackageProgress(signal_failure);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallFailedNotification()));
}

TEST_F(CrostiniPackageServiceTest, SecondUninstallDisplaysQueuedNotification) {
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(0, DEFAULT_APP),
                           IsUninstallQueuedNotification(SECOND_APP)));
}

TEST_F(CrostiniPackageServiceTest, SecondUninstallStartsWhenFirstCompletes) {
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);

  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallProgressNotification(0, SECOND_APP)));
}

TEST_F(CrostiniPackageServiceTest, SecondUninstallStartsWhenFirstFails) {
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);

  StartAndSignalUninstall(UninstallPackageProgressSignal::FAILED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallFailedNotification(DEFAULT_APP),
                           IsUninstallProgressNotification(0, SECOND_APP)));
}

TEST_F(CrostiniPackageServiceTest, DuplicateUninstallSucceeds) {
  // Use three uninstalls as a regression test for crbug.com/1015341
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  StartAndSignalUninstall(UninstallPackageProgressSignal::UNINSTALLING,
                          50 /*progress_percent*/, kDefaultAppFileId, &request);

  crostini_test_helper_->RemoveApp(0);

  UninstallPackageProgressSignal signal_success = MakeUninstallSignal(request);
  signal_success.set_status(UninstallPackageProgressSignal::SUCCEEDED);
  fake_cicerone_client_->UninstallPackageProgress(signal_success);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallSuccessNotification(DEFAULT_APP)));
}

TEST_F(CrostiniPackageServiceTest,
       AfterSecondInstallStartsProgressAppliesToSecond) {
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);

  // Uninstall the first.
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  // Start uninstalling the second
  StartAndSignalUninstall(UninstallPackageProgressSignal::UNINSTALLING,
                          46 /*progress_percent*/, kSecondAppFileId);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallProgressNotification(46, SECOND_APP)));
}

TEST_F(CrostiniPackageServiceTest, BothUninstallsEventuallyComplete) {
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);

  // Uninstall the first.
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  // Uninstall the second.
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED,
                          0 /*progress_percent*/, kSecondAppFileId);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallSuccessNotification(SECOND_APP)));
}

TEST_F(CrostiniPackageServiceTest, QueuedUninstallsProcessedInFifoOrder) {
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);
  service_->QueueUninstallApplication(kThirdAppId);
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(0, DEFAULT_APP),
                           IsUninstallQueuedNotification(SECOND_APP),
                           IsUninstallQueuedNotification(THIRD_APP)));

  // Finish the first; second should start.
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallProgressNotification(0, SECOND_APP),
                           IsUninstallQueuedNotification(THIRD_APP)));

  // Finish the second, third should start.
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED,
                          0 /*progress_percent*/, kSecondAppFileId);
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallSuccessNotification(SECOND_APP),
                           IsUninstallProgressNotification(0, THIRD_APP)));

  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED,
                          0 /*progress_percent*/, kThirdAppFileId);
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallSuccessNotification(SECOND_APP),
                           IsUninstallSuccessNotification(THIRD_APP)));
}

TEST_F(CrostiniPackageServiceTest, UninstallNotificationWaitsForAppListUpdate) {
  service_->QueueUninstallApplication(kDefaultAppId);

  SendAppListUpdateSignal(kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
                          1);

  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallWaitingForAppListNotification(DEFAULT_APP)));

  SendAppListUpdateSignal(kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
                          0);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP)));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallNotificationDoesntWaitForAppListUpdate) {
  service_->QueueUninstallApplication(kDefaultAppId);

  SendAppListUpdateSignal(kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
                          0);

  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP)));

  SendAppListUpdateSignal(kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
                          1);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP)));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallNotificationAppListUpdatesAreVmSpecific) {
  UninstallPackageOwningFileRequest request;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback;

  service_->QueueUninstallApplication(kDefaultAppId);
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);
  UninstallPackageProgressSignal signal_progress = MakeUninstallSignal(request);
  signal_progress.set_status(UninstallPackageProgressSignal::SUCCEEDED);

  service_->QueueUninstallApplication(kDifferentVmAppId);
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);
  UninstallPackageProgressSignal signal_progress2 =
      MakeUninstallSignal(request);
  signal_progress2.set_status(UninstallPackageProgressSignal::SUCCEEDED);

  SendAppListUpdateSignal(kDifferentVmVmName, kCrostiniDefaultContainerName, 1);
  fake_cicerone_client_->UninstallPackageProgress(signal_progress);
  fake_cicerone_client_->UninstallPackageProgress(signal_progress2);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallSuccessNotification(DEFAULT_APP),
          IsUninstallWaitingForAppListNotification(DIFFERENT_VM)));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallNotificationAppListUpdatesFromUnknownContainersAreIgnored) {
  service_->QueueUninstallApplication(kDefaultAppId);

  SendAppListUpdateSignal(kDifferentVmVmName, kCrostiniDefaultContainerName, 1);

  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP)));
}

TEST_F(CrostiniPackageServiceTest, UninstallNotificationFailsOnVmShutdown) {
  // Use two apps to ensure one is queued up.
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);

  base::RunLoop run_loop;
  CrostiniManager::GetForProfile(profile_.get())
      ->StopVm(kCrostiniDefaultVmName,
               base::BindOnce(
                   [](base::OnceClosure quit, crostini::CrostiniResult) {
                     std::move(quit).Run();
                   },
                   run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallFailedNotification(DEFAULT_APP),
                           IsUninstallFailedNotification(SECOND_APP)));
}

TEST_F(CrostiniPackageServiceTest, ClosingSuccessNotificationWorks) {
  service_->QueueUninstallApplication(kDefaultAppId);
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  std::vector<message_center::Notification> notifications =
      notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1U);
  EXPECT_THAT(Printable(notifications[0]), IsUninstallSuccessNotification());
  CloseNotification(notifications[0]);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      IsEmpty());
}

TEST_F(CrostiniPackageServiceTest, ClosingFailureNotificationWorks) {
  service_->QueueUninstallApplication(kDefaultAppId);
  StartAndSignalUninstall(UninstallPackageProgressSignal::FAILED);

  std::vector<message_center::Notification> notifications =
      notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1U);
  EXPECT_THAT(Printable(notifications[0]), IsUninstallFailedNotification());
  CloseNotification(notifications[0]);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      IsEmpty());
}

TEST_F(CrostiniPackageServiceTest,
       ClosedInProgressNotificationDoesNotReopenOnProgress) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);
  EXPECT_EQ(request.desktop_file_id(), kDefaultAppFileId);
  UninstallPackageOwningFileResponse response;
  response.set_status(UninstallPackageOwningFileResponse::STARTED);
  std::move(callback).Run(response);

  std::vector<message_center::Notification> notifications =
      notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1U);
  EXPECT_THAT(Printable(notifications[0]), IsUninstallProgressNotification(0));
  CloseNotification(notifications[0]);

  UninstallPackageProgressSignal signal_progress = MakeUninstallSignal(request);
  signal_progress.set_status(UninstallPackageProgressSignal::UNINSTALLING);
  signal_progress.set_progress_percent(50);
  fake_cicerone_client_->UninstallPackageProgress(signal_progress);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      IsEmpty());
}

TEST_F(CrostiniPackageServiceTest,
       ClosedInProgressNotificationReopensOnSuccess) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);
  EXPECT_EQ(request.desktop_file_id(), kDefaultAppFileId);
  UninstallPackageOwningFileResponse response;
  response.set_status(UninstallPackageOwningFileResponse::STARTED);
  std::move(callback).Run(response);

  std::vector<message_center::Notification> notifications =
      notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1U);
  EXPECT_THAT(Printable(notifications[0]), IsUninstallProgressNotification(0));
  CloseNotification(notifications[0]);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      IsEmpty());

  UninstallPackageProgressSignal signal_success = MakeUninstallSignal(request);
  signal_success.set_status(UninstallPackageProgressSignal::SUCCEEDED);
  fake_cicerone_client_->UninstallPackageProgress(signal_success);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       ClosedInProgressNotificationReopensOnFailure) {
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);
  EXPECT_EQ(request.desktop_file_id(), kDefaultAppFileId);
  UninstallPackageOwningFileResponse response;
  response.set_status(UninstallPackageOwningFileResponse::STARTED);
  std::move(callback).Run(response);

  std::vector<message_center::Notification> notifications =
      notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1U);
  EXPECT_THAT(Printable(notifications[0]), IsUninstallProgressNotification(0));
  CloseNotification(notifications[0]);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      IsEmpty());

  UninstallPackageProgressSignal signal_failure = MakeUninstallSignal(request);
  signal_failure.set_status(UninstallPackageProgressSignal::FAILED);
  signal_failure.set_failure_details("I prefer not to");
  fake_cicerone_client_->UninstallPackageProgress(signal_failure);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallFailedNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       ClosedQueuedNotificationDoesNotReopenOnProgress) {
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);

  std::vector<message_center::Notification> notifications =
      notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 2U);
  EXPECT_THAT(Printable(notifications[1]),
              IsUninstallQueuedNotification(SECOND_APP));
  CloseNotification(notifications[1]);

  // Complete Uninstall 1
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  // Uninstall 2 is now started, but we don't see a notification.
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP)));
}

TEST_F(CrostiniPackageServiceTest,
       ClosedQueuedNotificationDoesNotReopenOnFurtherProgress) {
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);

  std::vector<message_center::Notification> notifications =
      notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 2U);
  EXPECT_THAT(Printable(notifications[1]),
              IsUninstallQueuedNotification(SECOND_APP));
  CloseNotification(notifications[1]);

  // Complete Uninstall 1
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  // Start Uninstall 2
  StartAndSignalUninstall(UninstallPackageProgressSignal::UNINSTALLING,
                          50 /*progress_percent*/, kSecondAppFileId);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP)));
}

TEST_F(CrostiniPackageServiceTest,
       ClosedQueuedNotificationReopensOnCompletion) {
  service_->QueueUninstallApplication(kDefaultAppId);
  service_->QueueUninstallApplication(kSecondAppId);

  std::vector<message_center::Notification> notifications =
      notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 2U);
  EXPECT_THAT(Printable(notifications[1]),
              IsUninstallQueuedNotification(SECOND_APP));
  CloseNotification(notifications[1]);

  // Complete Uninstall 1
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED);

  // Complete Uninstall 2
  StartAndSignalUninstall(UninstallPackageProgressSignal::SUCCEEDED,
                          0 /*progress_percent*/, kSecondAppFileId);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallSuccessNotification(SECOND_APP)));
}

TEST_F(CrostiniPackageServiceTest, UninstallsOnDifferentVmsDoNotInterfere) {
  service_->QueueUninstallApplication(kDefaultAppId);
  UninstallPackageOwningFileRequest request;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);
  EXPECT_EQ(request.vm_name(), kCrostiniDefaultVmName);
  EXPECT_EQ(request.desktop_file_id(), kDefaultAppFileId);

  service_->QueueUninstallApplication(kDifferentVmAppId);
  UninstallPackageOwningFileRequest request_different_vm;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback_different_vm;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request_different_vm,
                               &callback_different_vm);
  EXPECT_EQ(request_different_vm.vm_name(), kDifferentVmVmName);
  EXPECT_EQ(request_different_vm.desktop_file_id(), kDifferentVmAppFileId);
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(0, DEFAULT_APP),
                           IsUninstallProgressNotification(0, DIFFERENT_VM)));

  UninstallPackageOwningFileResponse response;
  response.set_status(UninstallPackageOwningFileResponse::STARTED);
  std::move(callback).Run(response);
  UninstallPackageProgressSignal signal_progress = MakeUninstallSignal(request);
  signal_progress.set_status(UninstallPackageProgressSignal::UNINSTALLING);
  signal_progress.set_progress_percent(60);
  fake_cicerone_client_->UninstallPackageProgress(signal_progress);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(60, DEFAULT_APP),
                           IsUninstallProgressNotification(0, DIFFERENT_VM)));

  std::move(callback_different_vm).Run(response);
  signal_progress = MakeUninstallSignal(request_different_vm);
  signal_progress.set_status(UninstallPackageProgressSignal::UNINSTALLING);
  signal_progress.set_progress_percent(40);
  fake_cicerone_client_->UninstallPackageProgress(signal_progress);
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(60, DEFAULT_APP),
                           IsUninstallProgressNotification(40, DIFFERENT_VM)));

  UninstallPackageProgressSignal signal_success =
      MakeUninstallSignal(request_different_vm);
  signal_success.set_status(UninstallPackageProgressSignal::SUCCEEDED);
  fake_cicerone_client_->UninstallPackageProgress(signal_success);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(60, DEFAULT_APP),
                           IsUninstallSuccessNotification(DIFFERENT_VM)));

  UninstallPackageProgressSignal signal_failure = MakeUninstallSignal(request);
  signal_failure.set_status(UninstallPackageProgressSignal::FAILED);
  signal_failure.set_failure_details("Nope");
  fake_cicerone_client_->UninstallPackageProgress(signal_failure);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallFailedNotification(DEFAULT_APP),
                           IsUninstallSuccessNotification(DIFFERENT_VM)));
}

TEST_F(CrostiniPackageServiceTest, UninstallsOnDifferentVmsHaveSeparateQueues) {
  service_->QueueUninstallApplication(kDefaultAppId);
  UninstallPackageOwningFileRequest request_default;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback_default;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request_default,
                               &callback_default);
  EXPECT_EQ(request_default.vm_name(), kCrostiniDefaultVmName);
  EXPECT_EQ(request_default.desktop_file_id(), kDefaultAppFileId);

  service_->QueueUninstallApplication(kDifferentVmAppId);
  UninstallPackageOwningFileRequest request_different_vm;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback_different_vm;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request_different_vm,
                               &callback_different_vm);
  EXPECT_EQ(request_different_vm.vm_name(), kDifferentVmVmName);
  EXPECT_EQ(request_different_vm.desktop_file_id(), kDifferentVmAppFileId);

  service_->QueueUninstallApplication(kSecondAppId);
  service_->QueueUninstallApplication(kDifferentVmApp2Id);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(0, DEFAULT_APP),
                           IsUninstallProgressNotification(0, DIFFERENT_VM),
                           IsUninstallQueuedNotification(SECOND_APP),
                           IsUninstallQueuedNotification(DIFFERENT_VM_2)));

  UninstallPackageOwningFileResponse response;
  response.set_status(UninstallPackageOwningFileResponse::FAILED);
  std::move(callback_different_vm).Run(response);

  // Even though kSecondAppId was queued first, kDifferentVmApp2Id is moved
  // to progress state, because it's on a different queue.
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(0, DEFAULT_APP),
                           IsUninstallFailedNotification(DIFFERENT_VM),
                           IsUninstallQueuedNotification(SECOND_APP),
                           IsUninstallProgressNotification(0, DIFFERENT_VM_2)));

  UninstallPackageOwningFileRequest request_different_vm_2;
  DBusMethodCallback<UninstallPackageOwningFileResponse>
      callback_different_vm_2;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request_different_vm_2,
                               &callback_different_vm_2);
  EXPECT_EQ(request_different_vm_2.vm_name(), kDifferentVmVmName);
  EXPECT_EQ(request_different_vm_2.desktop_file_id(), kDifferentVmApp2FileId);
  response.set_status(UninstallPackageOwningFileResponse::STARTED);
  std::move(callback_different_vm_2).Run(response);
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(0, DEFAULT_APP),
                           IsUninstallFailedNotification(DIFFERENT_VM),
                           IsUninstallQueuedNotification(SECOND_APP),
                           IsUninstallProgressNotification(0, DIFFERENT_VM_2)));

  UninstallPackageProgressSignal signal_success =
      MakeUninstallSignal(request_different_vm_2);
  signal_success.set_status(UninstallPackageProgressSignal::SUCCEEDED);
  fake_cicerone_client_->UninstallPackageProgress(signal_success);

  // Even though the task finished on VM 2, SECOND_APP does not start
  // uninstalling because it is on a different queue.
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallProgressNotification(0, DEFAULT_APP),
                           IsUninstallFailedNotification(DIFFERENT_VM),
                           IsUninstallQueuedNotification(SECOND_APP),
                           IsUninstallSuccessNotification(DIFFERENT_VM_2)));

  std::move(callback_default).Run(response);
  signal_success = MakeUninstallSignal(request_default);
  signal_success.set_status(UninstallPackageProgressSignal::SUCCEEDED);
  fake_cicerone_client_->UninstallPackageProgress(signal_success);

  // Only when the uninstall on the default VM is done does SECOND_APP start
  // uninstalling.
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsUninstallSuccessNotification(DEFAULT_APP),
                           IsUninstallFailedNotification(DIFFERENT_VM),
                           IsUninstallProgressNotification(0, SECOND_APP),
                           IsUninstallSuccessNotification(DIFFERENT_VM_2)));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallsOnDifferentContainersDoNotInterfere) {
  service_->QueueUninstallApplication(kDefaultAppId);
  UninstallPackageOwningFileRequest request;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, &callback);
  EXPECT_EQ(request.container_name(), kCrostiniDefaultContainerName);
  EXPECT_EQ(request.desktop_file_id(), kDefaultAppFileId);

  service_->QueueUninstallApplication(kDifferentContainerAppId);
  UninstallPackageOwningFileRequest request_different_container;
  DBusMethodCallback<UninstallPackageOwningFileResponse>
      callback_different_container;
  RunUntilUninstallRequestMade(fake_cicerone_client_,
                               &request_different_container,
                               &callback_different_container);
  EXPECT_EQ(request_different_container.container_name(),
            kDifferentContainerContainerName);
  EXPECT_EQ(request_different_container.desktop_file_id(),
            kDifferentContainerAppFileId);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallProgressNotification(0, DEFAULT_APP),
          IsUninstallProgressNotification(0, DIFFERENT_CONTAINER)));

  UninstallPackageOwningFileResponse response;
  response.set_status(UninstallPackageOwningFileResponse::STARTED);
  std::move(callback).Run(response);
  UninstallPackageProgressSignal signal_progress = MakeUninstallSignal(request);
  signal_progress.set_status(UninstallPackageProgressSignal::UNINSTALLING);
  signal_progress.set_progress_percent(60);
  fake_cicerone_client_->UninstallPackageProgress(signal_progress);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallProgressNotification(60, DEFAULT_APP),
          IsUninstallProgressNotification(0, DIFFERENT_CONTAINER)));

  std::move(callback_different_container).Run(response);
  signal_progress = MakeUninstallSignal(request_different_container);
  signal_progress.set_status(UninstallPackageProgressSignal::UNINSTALLING);
  signal_progress.set_progress_percent(40);
  fake_cicerone_client_->UninstallPackageProgress(signal_progress);
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallProgressNotification(60, DEFAULT_APP),
          IsUninstallProgressNotification(40, DIFFERENT_CONTAINER)));

  UninstallPackageProgressSignal signal_success =
      MakeUninstallSignal(request_different_container);
  signal_success.set_status(UninstallPackageProgressSignal::SUCCEEDED);
  fake_cicerone_client_->UninstallPackageProgress(signal_success);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallProgressNotification(60, DEFAULT_APP),
          IsUninstallSuccessNotification(DIFFERENT_CONTAINER)));

  UninstallPackageProgressSignal signal_failure = MakeUninstallSignal(request);
  signal_failure.set_status(UninstallPackageProgressSignal::FAILED);
  signal_failure.set_failure_details("Nope");
  fake_cicerone_client_->UninstallPackageProgress(signal_failure);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallFailedNotification(DEFAULT_APP),
          IsUninstallSuccessNotification(DIFFERENT_CONTAINER)));
}

TEST_F(CrostiniPackageServiceTest,
       UninstallsOnDifferentContainersHaveSeparateQueues) {
  service_->QueueUninstallApplication(kDefaultAppId);
  UninstallPackageOwningFileRequest request_default;
  DBusMethodCallback<UninstallPackageOwningFileResponse> callback_default;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request_default,
                               &callback_default);
  EXPECT_EQ(request_default.container_name(), kCrostiniDefaultContainerName);
  EXPECT_EQ(request_default.desktop_file_id(), kDefaultAppFileId);

  service_->QueueUninstallApplication(kDifferentContainerAppId);
  UninstallPackageOwningFileRequest request_different_container;
  DBusMethodCallback<UninstallPackageOwningFileResponse>
      callback_different_container;
  RunUntilUninstallRequestMade(fake_cicerone_client_,
                               &request_different_container,
                               &callback_different_container);
  EXPECT_EQ(request_different_container.container_name(),
            kDifferentContainerContainerName);
  EXPECT_EQ(request_different_container.desktop_file_id(),
            kDifferentContainerAppFileId);

  service_->QueueUninstallApplication(kSecondAppId);
  service_->QueueUninstallApplication(kDifferentContainerApp2Id);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallProgressNotification(0, DEFAULT_APP),
          IsUninstallProgressNotification(0, DIFFERENT_CONTAINER),
          IsUninstallQueuedNotification(SECOND_APP),
          IsUninstallQueuedNotification(DIFFERENT_CONTAINER_2)));

  UninstallPackageOwningFileResponse response;
  response.set_status(UninstallPackageOwningFileResponse::FAILED);
  std::move(callback_different_container).Run(response);

  // Even though kSecondAppId was queued first, kDifferentContainerApp2Id is
  // moved to progress state, because it's on a different queue.
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallProgressNotification(0, DEFAULT_APP),
          IsUninstallFailedNotification(DIFFERENT_CONTAINER),
          IsUninstallQueuedNotification(SECOND_APP),
          IsUninstallProgressNotification(0, DIFFERENT_CONTAINER_2)));

  UninstallPackageOwningFileRequest request_different_container_2;
  DBusMethodCallback<UninstallPackageOwningFileResponse>
      callback_different_container_2;
  RunUntilUninstallRequestMade(fake_cicerone_client_,
                               &request_different_container_2,
                               &callback_different_container_2);
  EXPECT_EQ(request_different_container_2.container_name(),
            kDifferentContainerContainerName);
  EXPECT_EQ(request_different_container_2.desktop_file_id(),
            kDifferentContainerApp2FileId);
  response.set_status(UninstallPackageOwningFileResponse::STARTED);
  std::move(callback_different_container_2).Run(response);
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallProgressNotification(0, DEFAULT_APP),
          IsUninstallFailedNotification(DIFFERENT_CONTAINER),
          IsUninstallQueuedNotification(SECOND_APP),
          IsUninstallProgressNotification(0, DIFFERENT_CONTAINER_2)));

  UninstallPackageProgressSignal signal_success =
      MakeUninstallSignal(request_different_container_2);
  signal_success.set_status(UninstallPackageProgressSignal::SUCCEEDED);
  fake_cicerone_client_->UninstallPackageProgress(signal_success);

  // Even though the task finished on container 2, SECOND_APP does not start
  // uninstalling because it is on a different queue.
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallProgressNotification(0, DEFAULT_APP),
          IsUninstallFailedNotification(DIFFERENT_CONTAINER),
          IsUninstallQueuedNotification(SECOND_APP),
          IsUninstallSuccessNotification(DIFFERENT_CONTAINER_2)));

  std::move(callback_default).Run(response);
  signal_success = MakeUninstallSignal(request_default);
  signal_success.set_status(UninstallPackageProgressSignal::SUCCEEDED);
  fake_cicerone_client_->UninstallPackageProgress(signal_success);

  // Only when the uninstall on the default container is done does SECOND_APP
  // start uninstalling.
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(
          IsUninstallSuccessNotification(DEFAULT_APP),
          IsUninstallFailedNotification(DIFFERENT_CONTAINER),
          IsUninstallProgressNotification(0, SECOND_APP),
          IsUninstallSuccessNotification(DIFFERENT_CONTAINER_2)));
}

TEST_F(CrostiniPackageServiceTest, InstallSendsValidRequest) {
  base::RunLoop run_loop;
  service_->QueueInstallLinuxPackage(
      kDifferentVmVmName, kDifferentContainerContainerName, package_file_url_,
      base::BindOnce(&ExpectedCrostiniResult, run_loop.QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop.Run();

  const vm_tools::cicerone::InstallLinuxPackageRequest& request =
      fake_cicerone_client_->get_most_recent_install_linux_package_request();

  EXPECT_EQ(request.vm_name(), kDifferentVmVmName);
  EXPECT_EQ(request.container_name(), kDifferentContainerContainerName);
  EXPECT_EQ(request.owner_id(), CryptohomeIdForProfile(profile_.get()));
  EXPECT_EQ(request.file_path(), kPackageFileContainerPath);
}

TEST_F(CrostiniPackageServiceTest, InstallConvertPathFailure) {
  base::RunLoop run_loop;
  service_->QueueInstallLinuxPackage(
      kDifferentVmVmName, kDifferentContainerContainerName,
      storage::FileSystemURL::CreateForTest(GURL("invalid")),
      base::BindOnce(&ExpectedCrostiniResult, run_loop.QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop.Run();
}

TEST_F(CrostiniPackageServiceTest, InstallDisplaysProgressNotificationOnStart) {
  base::RunLoop run_loop;
  service_->QueueInstallLinuxPackage(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName, package_file_url_,
      base::BindOnce(&ExpectedCrostiniResult, run_loop.QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop.Run();

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallProgressNotification(0)));
}

TEST_F(CrostiniPackageServiceTest,
       InstallUpdatesProgressNotificationOnDownloadingSignal) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  StartAndSignalInstall(InstallLinuxPackageProgressSignal::DOWNLOADING,
                        44 /*progress_percent*/);

  // 22 = 44/2
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallProgressNotification(22)));
}

TEST_F(CrostiniPackageServiceTest,
       InstallUpdatesProgressNotificationOnInstallingSignal) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  StartAndSignalInstall(InstallLinuxPackageProgressSignal::INSTALLING,
                        44 /*progress_percent*/);

  // 72 = 44/2 + 50
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallProgressNotification(72)));
}

TEST_F(CrostiniPackageServiceTest,
       InstallDisplaysSuccessNotificationOnSuccessSignal) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  StartAndSignalInstall(InstallLinuxPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallSuccessNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       InstallDisplaysFailureNotificationOnFailedSignal) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  StartAndSignalInstall(InstallLinuxPackageProgressSignal::FAILED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallFailedNotification()));
}

TEST_F(CrostiniPackageServiceTest, InstallNotificationWaitsForAppListUpdate) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  SendAppListUpdateSignal(kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
                          1);

  StartAndSignalInstall(InstallLinuxPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallWaitingForAppListNotification()));

  SendAppListUpdateSignal(kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
                          0);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallSuccessNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       InstallNotificationDoesntWaitForAppListUpdate) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  SendAppListUpdateSignal(kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
                          0);

  StartAndSignalInstall(InstallLinuxPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallSuccessNotification()));

  SendAppListUpdateSignal(kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
                          1);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallSuccessNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       InstallNotificationAppListUpdatesAreVmSpecific) {
  InstallLinuxPackageRequest request;

  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  request =
      fake_cicerone_client_->get_most_recent_install_linux_package_request();
  InstallLinuxPackageProgressSignal signal_progress =
      MakeInstallSignal(request);
  signal_progress.set_status(InstallLinuxPackageProgressSignal::SUCCEEDED);

  service_->QueueInstallLinuxPackage(kDifferentVmVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  request =
      fake_cicerone_client_->get_most_recent_install_linux_package_request();
  InstallLinuxPackageProgressSignal signal_progress2 =
      MakeInstallSignal(request);
  signal_progress2.set_status(InstallLinuxPackageProgressSignal::SUCCEEDED);

  base::RunLoop().RunUntilIdle();

  SendAppListUpdateSignal(kDifferentVmVmName, kCrostiniDefaultContainerName, 1);
  fake_cicerone_client_->InstallLinuxPackageProgress(signal_progress);
  fake_cicerone_client_->InstallLinuxPackageProgress(signal_progress2);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallSuccessNotification(),
                           IsInstallWaitingForAppListNotification()));
}

TEST_F(CrostiniPackageServiceTest,
       InstallNotificationAppListUpdatesFromUnknownContainersAreIgnored) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());

  base::RunLoop().RunUntilIdle();

  SendAppListUpdateSignal(kDifferentVmVmName, kCrostiniDefaultContainerName, 1);

  StartAndSignalInstall(InstallLinuxPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallSuccessNotification()));
}

TEST_F(CrostiniPackageServiceTest, InstallNotificationFailsOnVmShutdown) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());

  base::RunLoop().RunUntilIdle();

  StartAndSignalInstall(InstallLinuxPackageProgressSignal::INSTALLING);

  base::RunLoop run_loop;
  CrostiniManager::GetForProfile(profile_.get())
      ->StopVm(kCrostiniDefaultVmName,
               base::BindOnce(
                   [](base::OnceClosure quit, crostini::CrostiniResult) {
                     std::move(quit).Run();
                   },
                   run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallFailedNotification()));
}

TEST_F(CrostiniPackageServiceTest, UninstallsQueuesBehindStartingUpInstall) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  service_->QueueUninstallApplication(kDefaultAppId);

  // Install doesn't show a notification until it gets a response, but uninstall
  // still shows a queued notification.
  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallProgressNotification(0),
                           IsUninstallQueuedNotification()));
}

TEST_F(CrostiniPackageServiceTest, InstallRunsInFrontOfQueuedUninstall) {
  base::RunLoop run_loop;
  service_->QueueInstallLinuxPackage(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName, package_file_url_,
      base::BindOnce(&ExpectedCrostiniResult, run_loop.QuitClosure(),
                     CrostiniResult::SUCCESS));
  service_->QueueUninstallApplication(kDefaultAppId);
  run_loop.Run();

  // Ensure the install started, not the uninstall.
  const vm_tools::cicerone::InstallLinuxPackageRequest& request =
      fake_cicerone_client_->get_most_recent_install_linux_package_request();
  EXPECT_EQ(request.file_path(), kPackageFileContainerPath);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallProgressNotification(0),
                           IsUninstallQueuedNotification()));
}

TEST_F(CrostiniPackageServiceTest, QueuedUninstallRunsAfterCompletedInstall) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  service_->QueueUninstallApplication(kDefaultAppId);
  StartAndSignalInstall(InstallLinuxPackageProgressSignal::SUCCEEDED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallSuccessNotification(),
                           IsUninstallProgressNotification(0)));
  UninstallPackageOwningFileRequest request;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, nullptr);

  EXPECT_EQ(request.vm_name(), kCrostiniDefaultVmName);
  EXPECT_EQ(request.container_name(), kCrostiniDefaultContainerName);
  EXPECT_EQ(request.owner_id(), CryptohomeIdForProfile(profile_.get()));
  EXPECT_EQ(request.desktop_file_id(), kDefaultAppFileId);
}

TEST_F(CrostiniPackageServiceTest,
       QueuedUninstallRunsAfterFailedToStartInstall) {
  InstallLinuxPackageResponse response;
  response.set_status(InstallLinuxPackageResponse::FAILED);
  response.set_failure_reason("No such file");
  fake_cicerone_client_->set_install_linux_package_response(response);
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  service_->QueueUninstallApplication(kDefaultAppId);

  UninstallPackageOwningFileRequest request;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, nullptr);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallFailedNotification(),
                           IsUninstallProgressNotification(0)));

  EXPECT_EQ(request.vm_name(), kCrostiniDefaultVmName);
  EXPECT_EQ(request.container_name(), kCrostiniDefaultContainerName);
  EXPECT_EQ(request.owner_id(), CryptohomeIdForProfile(profile_.get()));
  EXPECT_EQ(request.desktop_file_id(), kDefaultAppFileId);
}

TEST_F(CrostiniPackageServiceTest,
       QueuedUninstallRunsAfterFailedInstallSignal) {
  service_->QueueInstallLinuxPackage(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName,
                                     package_file_url_, base::DoNothing());
  service_->QueueUninstallApplication(kDefaultAppId);
  StartAndSignalInstall(InstallLinuxPackageProgressSignal::FAILED);

  EXPECT_THAT(
      Printable(notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT)),
      UnorderedElementsAre(IsInstallFailedNotification(),
                           IsUninstallProgressNotification(0)));
  UninstallPackageOwningFileRequest request;
  RunUntilUninstallRequestMade(fake_cicerone_client_, &request, nullptr);

  EXPECT_EQ(request.vm_name(), kCrostiniDefaultVmName);
  EXPECT_EQ(request.container_name(), kCrostiniDefaultContainerName);
  EXPECT_EQ(request.owner_id(), CryptohomeIdForProfile(profile_.get()));
  EXPECT_EQ(request.desktop_file_id(), kDefaultAppFileId);
}

TEST_F(CrostiniPackageServiceTest, GetLinuxPackageInfoSendsCorrectRequest) {
  service_->GetLinuxPackageInfo(kDifferentVmVmName,
                                kDifferentContainerContainerName,
                                package_file_url_, base::DoNothing());

  base::RunLoop().RunUntilIdle();

  const LinuxPackageInfoRequest& request =
      fake_cicerone_client_->get_most_recent_linux_package_info_request();
  EXPECT_EQ(request.vm_name(), kDifferentVmVmName);
  EXPECT_EQ(request.container_name(), kDifferentContainerContainerName);
  EXPECT_EQ(request.owner_id(), CryptohomeIdForProfile(profile_.get()));
  EXPECT_EQ(request.file_path(), kPackageFileContainerPath);
  EXPECT_TRUE(fake_seneschal_client_->share_path_called());
}

TEST_F(CrostiniPackageServiceTest, GetLinuxPackageInfoReturnsInfoOnSuccess) {
  LinuxPackageInfoResponse response;
  response.set_success(true);
  response.set_package_id("nethack;3.6.1;x64;some data");
  response.set_license("ngpl");
  response.set_description("Explore the Dungeon!");
  response.set_project_url("https://www.nethack.org/");
  response.set_size(548422342432);
  response.set_summary("Fight! Run! Win!");
  fake_cicerone_client_->set_linux_package_info_response(response);

  LinuxPackageInfo result;
  service_->GetLinuxPackageInfo(
      kDifferentVmVmName, kDifferentContainerContainerName, package_file_url_,
      base::BindOnce(&RecordPackageInfoResult, base::Unretained(&result)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.name, "nethack");
  EXPECT_EQ(result.version, "3.6.1");
  EXPECT_EQ(result.summary, response.summary());
  EXPECT_EQ(result.description, response.description());
}

TEST_F(CrostiniPackageServiceTest, GetLinuxPackageInfoConvertPathFailure) {
  SharePathResponse response;
  response.set_success(false);
  fake_seneschal_client_->set_share_path_response(response);

  LinuxPackageInfo result;
  service_->GetLinuxPackageInfo(
      kDifferentVmVmName, kDifferentContainerContainerName,
      storage::FileSystemURL::CreateForTest(GURL("invalid")),
      base::BindOnce(&RecordPackageInfoResult, base::Unretained(&result)));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(result.success);
  EXPECT_TRUE(base::StartsWith(result.failure_reason, "Invalid package url:",
                               base::CompareCase::SENSITIVE));
}

TEST_F(CrostiniPackageServiceTest, GetLinuxPackageInfoSharePathFailure) {
  SharePathResponse response;
  response.set_success(false);
  fake_seneschal_client_->set_share_path_response(response);

  LinuxPackageInfo result;
  service_->GetLinuxPackageInfo(
      kDifferentVmVmName, kDifferentContainerContainerName, package_file_url_,
      base::BindOnce(&RecordPackageInfoResult, base::Unretained(&result)));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(result.success);
  EXPECT_TRUE(base::StartsWith(result.failure_reason, "Error sharing package",
                               base::CompareCase::SENSITIVE));
}

TEST_F(CrostiniPackageServiceTest, GetLinuxPackageInfoReturnsFailureOnFailure) {
  LinuxPackageInfoResponse response;
  response.set_success(false);
  response.set_failure_reason("test failure reason");
  fake_cicerone_client_->set_linux_package_info_response(response);

  LinuxPackageInfo result;
  service_->GetLinuxPackageInfo(
      kDifferentVmVmName, kDifferentContainerContainerName, package_file_url_,
      base::BindOnce(&RecordPackageInfoResult, base::Unretained(&result)));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.failure_reason, "test failure reason");
}

}  // namespace

}  // namespace crostini
