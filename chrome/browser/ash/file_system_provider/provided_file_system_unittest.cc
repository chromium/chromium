// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/provided_file_system.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/values.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/notification_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_observer.h"
#include "chrome/browser/ash/file_system_provider/request_manager.h"
#include "chrome/browser/ash/file_system_provider/watcher.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension_id.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using base::test::RunOnceCallback;
using testing::_;
using testing::IsEmpty;
using testing::SizeIs;

const char kOrigin[] =
    "chrome-extension://abcabcabcabcabcabcabcabcabcabcabcabca/";
const char kAnotherOrigin[] =
    "chrome-extension://efgefgefgefgefgefgefgefgefgefgefgefge/";
const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "camera-pictures";
const char kDisplayName[] = "Camera Pictures";
const base::FilePath::CharType kDirectoryPath[] =
    FILE_PATH_LITERAL("/hello/world");
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("/welcome/to/my/world");

// Fake implementation of the event router, mocking out a real extension.
// Handles requests and replies with fake answers back to the file system via
// the request manager.
class FakeEventRouter : public extensions::EventRouter {
 public:
  FakeEventRouter(Profile* profile, ProvidedFileSystemInterface* file_system)
      : EventRouter(profile, nullptr),
        file_system_(file_system),
        reply_result_(base::File::FILE_OK) {}

  FakeEventRouter(const FakeEventRouter&) = delete;
  FakeEventRouter& operator=(const FakeEventRouter&) = delete;

  ~FakeEventRouter() override = default;

  // Handles an event which would normally be routed to an extension. Instead
  // replies with a hard coded response.
  void DispatchEventToExtension(
      const extensions::ExtensionId& extension_id,
      std::unique_ptr<extensions::Event> event) override {
    ASSERT_TRUE(file_system_);
    const base::Value* dict = &event->event_args[0];
    ASSERT_TRUE(dict->is_dict());
    const std::string* file_system_id =
        dict->GetDict().FindString("fileSystemId");
    EXPECT_NE(file_system_id, nullptr);
    EXPECT_EQ(kFileSystemId, *file_system_id);
    std::optional<int> id = dict->GetDict().FindInt("requestId");
    EXPECT_TRUE(id);
    int request_id = *id;
    EXPECT_TRUE(event->event_name == extensions::api::file_system_provider::
                                         OnAddWatcherRequested::kEventName ||
                event->event_name == extensions::api::file_system_provider::
                                         OnRemoveWatcherRequested::kEventName ||
                event->event_name == extensions::api::file_system_provider::
                                         OnOpenFileRequested::kEventName ||
                event->event_name == extensions::api::file_system_provider::
                                         OnCloseFileRequested::kEventName);

    if (reply_result_ == base::File::FILE_OK) {
      base::Value::List list;
      list.Append(kFileSystemId);
      list.Append(request_id);
      list.Append(0);  // Execution time.

      using extensions::api::file_system_provider_internal::
          OperationRequestedSuccess::Params;
      std::optional<Params> params(Params::Create(list));
      ASSERT_TRUE(params.has_value());
      file_system_->GetRequestManager()->FulfillRequest(
          request_id,
          RequestValue::CreateForOperationSuccess(std::move(*params)),
          /*has_more=*/false);
    } else {
      file_system_->GetRequestManager()->RejectRequest(
          request_id, RequestValue(), reply_result_);
    }
  }

  void set_reply_result(base::File::Error result) { reply_result_ = result; }

 private:
  const raw_ptr<ProvidedFileSystemInterface,
                DanglingUntriaged>
      file_system_;  // Not owned.
  base::File::Error reply_result_;
};

class MockObserver : public ProvidedFileSystemObserver {
 public:
  MOCK_METHOD(void,
              OnWatcherChanged,
              (const ProvidedFileSystemInfo& file_system_info,
               const Watcher& watcher,
               storage::WatcherManager::ChangeType change_type,
               const ProvidedFileSystemObserver::Changes& changes,
               base::OnceClosure callback),
              (override));
  MOCK_METHOD(void,
              OnWatcherTagUpdated,
              (const ProvidedFileSystemInfo& file_system_info,
               const Watcher& watcher),
              (override));
  MOCK_METHOD(void,
              OnWatcherListChanged,
              (const ProvidedFileSystemInfo& file_system_info,
               const Watchers& watchers),
              (override));
};

// Stub notification manager, which works in unit tests.
class StubNotificationManager : public NotificationManagerInterface {
 public:
  StubNotificationManager() = default;

  StubNotificationManager(const StubNotificationManager&) = delete;
  StubNotificationManager& operator=(const StubNotificationManager&) = delete;

  ~StubNotificationManager() override = default;

  // NotificationManagerInterface overrides.
  void ShowUnresponsiveNotification(int id,
                                    NotificationCallback callback) override {}
  void HideUnresponsiveNotification(int id) override {}
};

typedef std::vector<base::File::Error> Log;
typedef std::vector<storage::WatcherManager::ChangeType> NotificationLog;
typedef std::vector<std::pair<int, base::File::Error>> OpenFileLog;

// Writes a |result| to the |log| vector.
void LogStatus(Log* log, base::File::Error result) {
  log->push_back(result);
}

// Writes a |change_type| to the |notification_log| vector.
void LogNotification(NotificationLog* notification_log,
                     storage::WatcherManager::ChangeType change_type) {
  notification_log->push_back(change_type);
}

// Writes a |file_handle| and |result| to the |open_file_log| vector.
void LogOpenFile(OpenFileLog* open_file_log,
                 int file_handle,
                 base::File::Error result,
                 std::unique_ptr<EntryMetadata> metadata) {
  open_file_log->emplace_back(file_handle, result);
}

}  // namespace

class FileSystemProviderProvidedFileSystemTest : public testing::Test {
 protected:
  FileSystemProviderProvidedFileSystemTest() = default;
  ~FileSystemProviderProvidedFileSystemTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    render_process_host_ =
        std::make_unique<content::MockRenderProcessHost>(profile_.get());
    const base::FilePath mount_path = util::GetMountPath(
        profile_.get(), ProviderId::CreateFromExtensionId(kExtensionId),
        kFileSystemId);
    MountOptions mount_options;
    mount_options.file_system_id = kFileSystemId;
    mount_options.display_name = kDisplayName;
    mount_options.supports_notify_tag = true;
    mount_options.writable = true;
    file_system_info_ = std::make_unique<ProvidedFileSystemInfo>(
        kExtensionId, mount_options, mount_path, /*configurable=*/false,
        /*watchable=*/true, extensions::SOURCE_FILE, IconSet());
    provided_file_system_ = std::make_unique<ProvidedFileSystem>(
        profile_.get(), *file_system_info_.get());
    event_router_ = std::make_unique<FakeEventRouter>(
        profile_.get(), provided_file_system_.get());
    event_router_->AddEventListener(extensions::api::file_system_provider::
                                        OnAddWatcherRequested::kEventName,
                                    render_process_host_.get(), kExtensionId);
    event_router_->AddEventListener(extensions::api::file_system_provider::
                                        OnRemoveWatcherRequested::kEventName,
                                    render_process_host_.get(), kExtensionId);
    event_router_->AddEventListener(
        extensions::api::file_system_provider::OnOpenFileRequested::kEventName,
        render_process_host_.get(), kExtensionId);
    event_router_->AddEventListener(
        extensions::api::file_system_provider::OnCloseFileRequested::kEventName,
        render_process_host_.get(), kExtensionId);
    provided_file_system_->SetEventRouterForTesting(event_router_.get());
    provided_file_system_->SetNotificationManagerForTesting(
        base::WrapUnique(new StubNotificationManager));
  }

  void TearDown() override {
    render_process_host_.reset();
    Test::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::RenderProcessHost> render_process_host_;
  std::unique_ptr<FakeEventRouter> event_router_;
  std::unique_ptr<ProvidedFileSystemInfo> file_system_info_;
  std::unique_ptr<ProvidedFileSystem> provided_file_system_;
};

TEST_F(FileSystemProviderProvidedFileSystemTest, AutoUpdater) {
  Log log;
  base::OnceClosure first_callback;
  base::OnceClosure second_callback;

  {
    // Auto updater is ref counted, and bound to all callbacks.
    scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(base::BindOnce(
        &LogStatus, base::Unretained(&log), base::File::FILE_OK)));

    first_callback = auto_updater->CreateCallback();
    second_callback = auto_updater->CreateCallback();
  }

  // Getting out of scope, should not invoke updating if there are pending
  // callbacks.
  EXPECT_EQ(0u, log.size());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(first_callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, log.size());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(second_callback));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, log.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AutoUpdater_NoCallbacks) {
  Log log;
  {
    scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(base::BindOnce(
        &LogStatus, base::Unretained(&log), base::File::FILE_OK)));
  }
  EXPECT_EQ(1u, log.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AutoUpdater_CallbackIgnored) {
  Log log;
  {
    scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(base::BindOnce(
        &LogStatus, base::Unretained(&log), base::File::FILE_OK)));
    base::OnceClosure callback = auto_updater->CreateCallback();
    // The callback gets out of scope, so the ref counted auto updater instance
    // gets deleted. Still, updating shouldn't be invoked, since the callback
    // wasn't executed.
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, log.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AddWatcher_NotFound) {
  Log log;
  NotificationLog notification_log;
  MockObserver mock_observer;

  provided_file_system_->AddObserver(&mock_observer);

  // First, set the extension response to an error.
  event_router_->set_reply_result(base::File::FILE_ERROR_NOT_FOUND);

  // The observer should not be called.
  EXPECT_CALL(mock_observer, OnWatcherListChanged(_, _)).Times(0);
  EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

  provided_file_system_->AddWatcher(
      GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
      /*persistent=*/false, base::BindOnce(&LogStatus, base::Unretained(&log)),
      base::BindRepeating(&LogNotification,
                          base::Unretained(&notification_log)));
  base::RunLoop().RunUntilIdle();

  // The directory should not become watched because of an error.
  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, log[0]);
  EXPECT_EQ(0u, notification_log.size());

  Watchers* const watchers = provided_file_system_->GetWatchers();
  EXPECT_EQ(0u, watchers->size());

  provided_file_system_->RemoveObserver(&mock_observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AddWatcher) {
  Log log;
  MockObserver mock_observer;

  provided_file_system_->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(1))).Times(1);
  EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

  provided_file_system_->AddWatcher(
      GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
      /*persistent=*/true, base::BindOnce(&LogStatus, base::Unretained(&log)),
      storage::WatcherManager::NotificationCallback());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_OK, log[0]);

  Watchers* const watchers = provided_file_system_->GetWatchers();
  ASSERT_EQ(1u, watchers->size());
  const Watcher& watcher = watchers->begin()->second;
  EXPECT_EQ(kDirectoryPath, watcher.entry_path.value());
  EXPECT_FALSE(watcher.recursive);
  EXPECT_EQ("", watcher.last_tag);

  provided_file_system_->RemoveObserver(&mock_observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AddWatcher_PersistentIllegal) {
  {
    // Adding a persistent watcher with a notification callback is not allowed,
    // as it's basically impossible to restore the callback after a shutdown.
    Log log;
    NotificationLog notification_log;

    MockObserver mock_observer;
    provided_file_system_->AddObserver(&mock_observer);

    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, _)).Times(0);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        /*=persistent=*/true,
        base::BindOnce(&LogStatus, base::Unretained(&log)),
        base::BindRepeating(&LogNotification,
                            base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, log[0]);

    provided_file_system_->RemoveObserver(&mock_observer);
  }

  {
    // Adding a persistent watcher is not allowed if the file system doesn't
    // support the notify tag. It's because the notify tag is essential to be
    // able to recreate notification during shutdown.
    Log log;
    MockObserver mock_observer;

    // Create a provided file system interface, which does not support a notify
    // tag, though.
    const base::FilePath mount_path = util::GetMountPath(
        profile_.get(), ProviderId::CreateFromExtensionId(kExtensionId),
        kFileSystemId);
    MountOptions mount_options;
    mount_options.file_system_id = kFileSystemId;
    mount_options.display_name = kDisplayName;
    mount_options.supports_notify_tag = false;
    ProvidedFileSystemInfo file_system_info(
        kExtensionId, mount_options, mount_path, /*configurable=*/false,
        /*watchable=*/true, extensions::SOURCE_FILE, IconSet());
    ProvidedFileSystem simple_provided_file_system(profile_.get(),
                                                   file_system_info);
    simple_provided_file_system.SetEventRouterForTesting(event_router_.get());
    simple_provided_file_system.SetNotificationManagerForTesting(
        base::WrapUnique(new StubNotificationManager));

    simple_provided_file_system.AddObserver(&mock_observer);

    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, _)).Times(0);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    simple_provided_file_system.AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        /*persistent=*/true, base::BindOnce(&LogStatus, base::Unretained(&log)),
        storage::WatcherManager::NotificationCallback());
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, log[0]);

    simple_provided_file_system.RemoveObserver(&mock_observer);
  }
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AddWatcher_Exists) {
  MockObserver mock_observer;
  provided_file_system_->AddObserver(&mock_observer);

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(1))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // First watch a directory not recursively.
    Log log;
    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        /*persistent=*/true, base::BindOnce(&LogStatus, base::Unretained(&log)),
        storage::WatcherManager::NotificationCallback());
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);

    Watchers* const watchers = provided_file_system_->GetWatchers();
    ASSERT_TRUE(watchers);
    ASSERT_EQ(1u, watchers->size());
    const auto& watcher_it = watchers->find(
        WatcherKey(base::FilePath(kDirectoryPath), /*recursive=*/false));
    ASSERT_NE(watchers->end(), watcher_it);

    EXPECT_EQ(1u, watcher_it->second.subscribers.size());
    const auto& subscriber_it =
        watcher_it->second.subscribers.find(GURL(kOrigin));
    ASSERT_NE(watcher_it->second.subscribers.end(), subscriber_it);
    EXPECT_EQ(kOrigin, subscriber_it->second.origin.spec());
    EXPECT_TRUE(subscriber_it->second.persistent);
  }

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, _)).Times(0);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // Create another non-recursive observer. That should fail.
    Log log;
    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        /*persistent=*/true, base::BindOnce(&LogStatus, base::Unretained(&log)),
        storage::WatcherManager::NotificationCallback());
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_EXISTS, log[0]);
  }

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(2))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // Lastly, create another recursive observer. That should succeed.
    Log log;
    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/true,
        /*persistent=*/true, base::BindOnce(&LogStatus, base::Unretained(&log)),
        storage::WatcherManager::NotificationCallback());
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
  }

  provided_file_system_->RemoveObserver(&mock_observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AddWatcher_MultipleOrigins) {
  MockObserver mock_observer;
  provided_file_system_->AddObserver(&mock_observer);

  {
    // First watch a directory not recursively.
    Log log;
    NotificationLog notification_log;

    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(1))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        /*persistent=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)),
        base::BindRepeating(&LogNotification,
                            base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(0u, notification_log.size());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    ASSERT_TRUE(watchers);
    ASSERT_EQ(1u, watchers->size());
    const auto& watcher_it = watchers->find(
        WatcherKey(base::FilePath(kDirectoryPath), /*recursive=*/false));
    ASSERT_NE(watchers->end(), watcher_it);

    EXPECT_EQ(1u, watcher_it->second.subscribers.size());
    const auto& subscriber_it =
        watcher_it->second.subscribers.find(GURL(kOrigin));
    ASSERT_NE(watcher_it->second.subscribers.end(), subscriber_it);
    EXPECT_EQ(kOrigin, subscriber_it->first.spec());
    EXPECT_EQ(kOrigin, subscriber_it->second.origin.spec());
    EXPECT_FALSE(subscriber_it->second.persistent);
  }

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(2))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // Create another watcher, but recursive and with a different origin.
    Log log;
    NotificationLog notification_log;

    provided_file_system_->AddWatcher(
        GURL(kAnotherOrigin), base::FilePath(kDirectoryPath),
        /*recursive=*/true, /*persistent=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)),
        base::BindRepeating(&LogNotification,
                            base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(0u, notification_log.size());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    ASSERT_TRUE(watchers);
    ASSERT_EQ(2u, watchers->size());
    const auto& watcher_it = watchers->find(
        WatcherKey(base::FilePath(kDirectoryPath), /*recursive=*/false));
    ASSERT_NE(watchers->end(), watcher_it);

    EXPECT_EQ(1u, watcher_it->second.subscribers.size());
    const auto& subscriber_it =
        watcher_it->second.subscribers.find(GURL(kOrigin));
    ASSERT_NE(watcher_it->second.subscribers.end(), subscriber_it);
    EXPECT_EQ(kOrigin, subscriber_it->first.spec());
    EXPECT_EQ(kOrigin, subscriber_it->second.origin.spec());
    EXPECT_FALSE(subscriber_it->second.persistent);
  }

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(1))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // Remove the second watcher gracefully.
    Log log;
    provided_file_system_->RemoveWatcher(
        GURL(kAnotherOrigin), base::FilePath(kDirectoryPath),
        /*recursive=*/true, base::BindOnce(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);

    Watchers* const watchers = provided_file_system_->GetWatchers();
    ASSERT_TRUE(watchers);
    EXPECT_EQ(1u, watchers->size());
    const auto& watcher_it = watchers->find(
        WatcherKey(base::FilePath(kDirectoryPath), /*recursive=*/false));
    ASSERT_NE(watchers->end(), watcher_it);

    EXPECT_EQ(1u, watcher_it->second.subscribers.size());
    const auto& subscriber_it =
        watcher_it->second.subscribers.find(GURL(kOrigin));
    ASSERT_NE(watcher_it->second.subscribers.end(), subscriber_it);
    EXPECT_EQ(kOrigin, subscriber_it->first.spec());
    EXPECT_EQ(kOrigin, subscriber_it->second.origin.spec());
    EXPECT_FALSE(subscriber_it->second.persistent);
  }

  provided_file_system_->RemoveObserver(&mock_observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, RemoveWatcher) {
  MockObserver mock_observer;
  provided_file_system_->AddObserver(&mock_observer);

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, _)).Times(0);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // First, confirm that removing a watcher which does not exist results in an
    // error.
    Log log;
    provided_file_system_->RemoveWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, log[0]);
  }

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(1))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // Watch a directory not recursively.
    Log log;
    NotificationLog notification_log;

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        /*persistent=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)),
        base::BindRepeating(&LogNotification,
                            base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(0u, notification_log.size());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(1u, watchers->size());
  }

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(0))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // Remove a watcher gracefully.
    Log log;
    provided_file_system_->RemoveWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(0u, watchers->size());
  }

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(1))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // Confirm that it's possible to watch it again.
    Log log;
    NotificationLog notification_log;

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        /*persistent=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)),
        base::BindRepeating(&LogNotification,
                            base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(0u, notification_log.size());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(1u, watchers->size());
  }

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(0))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // Finally, remove it, but with an error from extension. That should result
    // in a removed watcher, anyway. The error code should not be passed.
    event_router_->set_reply_result(base::File::FILE_ERROR_FAILED);

    Log log;
    provided_file_system_->RemoveWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(0u, watchers->size());
  }

  provided_file_system_->RemoveObserver(&mock_observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest,
       RemoveWatcher_NotifiedAfterWatchersRemoved) {
  MockObserver mock_observer;
  provided_file_system_->AddObserver(&mock_observer);

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(1))).Times(1);

    // Watch a directory not recursively.
    Log log;
    NotificationLog notification_log;

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        /*persistent=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)),
        base::BindRepeating(&LogNotification,
                            base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(1u, watchers->size());
  }

  {
    // The observer should be notified after the watchers list was modified, so
    // it should see 0 watchers.
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(0))).Times(1);

    Log log;
    provided_file_system_->RemoveWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();
  }

  provided_file_system_->RemoveObserver(&mock_observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, Notify) {
  MockObserver mock_observer;
  provided_file_system_->AddObserver(&mock_observer);
  NotificationLog notification_log;

  {
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(1))).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(0);

    // Watch a directory.
    Log log;

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), /*recursive=*/false,
        /*persistent=*/false,
        base::BindOnce(&LogStatus, base::Unretained(&log)),
        base::BindRepeating(&LogNotification,
                            base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(0u, notification_log.size());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(1u, watchers->size());
    provided_file_system_->GetWatchers();
    EXPECT_EQ("", watchers->begin()->second.last_tag);
  }

  {
    // Notify about a change.
    const storage::WatcherManager::ChangeType change_type =
        storage::WatcherManager::CHANGED;
    const std::string tag = "hello-world";

    EXPECT_CALL(mock_observer,
                OnWatcherChanged(_, _, change_type, IsEmpty(), _))
        .WillOnce([&](const ProvidedFileSystemInfo& file_system_info,
                      const Watcher& watcher,
                      storage::WatcherManager::ChangeType change_type,
                      const ProvidedFileSystemObserver::Changes& changes,
                      base::OnceClosure callback) {
          // The tag should not be updated in advance, before all observers
          // handle the notification.
          Watchers* const watchers = provided_file_system_->GetWatchers();
          EXPECT_EQ(1u, watchers->size());
          provided_file_system_->GetWatchers();
          EXPECT_EQ("", watchers->begin()->second.last_tag);

          // Mark the notification as handled.
          std::move(callback).Run();
        });

    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _))
        .WillOnce([&](const ProvidedFileSystemInfo& file_system_info,
                      const Watcher& watcher) {
          // Confirm, that the watcher still exists, and that the tag is
          // updated.
          Watchers* const watchers = provided_file_system_->GetWatchers();
          ASSERT_EQ(1u, watchers->size());
          EXPECT_EQ(tag, watchers->begin()->second.last_tag);
        });
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(1))).Times(0);

    Log log;
    provided_file_system_->Notify(
        base::FilePath(kDirectoryPath), /*recursive=*/false, change_type,
        base::WrapUnique(new ProvidedFileSystemObserver::Changes), tag,
        base::BindOnce(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    // Confirm that the notification callback was called.
    ASSERT_EQ(1u, notification_log.size());
    EXPECT_EQ(change_type, notification_log[0]);

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
  }

  {
    // Notify about deleting of the watched entry.
    const storage::WatcherManager::ChangeType change_type =
        storage::WatcherManager::DELETED;
    const std::string tag = "chocolate-disco";

    // Mark the notification as handled once received.
    EXPECT_CALL(mock_observer,
                OnWatcherChanged(_, _, change_type, IsEmpty(), _))
        .WillOnce(RunOnceCallback<4>());

    EXPECT_CALL(mock_observer, OnWatcherTagUpdated(_, _)).Times(1);
    EXPECT_CALL(mock_observer, OnWatcherListChanged(_, SizeIs(0))).Times(1);

    Log log;
    provided_file_system_->Notify(
        base::FilePath(kDirectoryPath), /*recursive=*/false, change_type,
        base::WrapUnique(new ProvidedFileSystemObserver::Changes), tag,
        base::BindOnce(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);

    // Confirm that the notification callback was called.
    ASSERT_EQ(2u, notification_log.size());
    EXPECT_EQ(change_type, notification_log[1]);
  }

  // Confirm, that the watcher is removed.
  {
    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(0u, watchers->size());
  }

  provided_file_system_->RemoveObserver(&mock_observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, OpenedFiles) {
  OpenFileLog log;
  provided_file_system_->OpenFile(
      base::FilePath(kFilePath), OPEN_FILE_MODE_WRITE,
      base::BindOnce(LogOpenFile, base::Unretained(&log)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_OK, log[0].second);
  const int file_handle = log[0].first;

  const OpenedFiles& opened_files = provided_file_system_->GetOpenedFiles();
  const auto opened_file_it = opened_files.find(file_handle);
  ASSERT_NE(opened_files.end(), opened_file_it);
  EXPECT_EQ(kFilePath, opened_file_it->second.file_path.AsUTF8Unsafe());
  EXPECT_EQ(OPEN_FILE_MODE_WRITE, opened_file_it->second.mode);

  Log close_log;
  provided_file_system_->CloseFile(
      file_handle, base::BindOnce(LogStatus, base::Unretained(&close_log)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, close_log.size());
  EXPECT_EQ(base::File::FILE_OK, close_log[0]);
  EXPECT_EQ(0u, opened_files.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, OpenedFiles_OpeningFailure) {
  event_router_->set_reply_result(base::File::FILE_ERROR_NOT_FOUND);

  OpenFileLog log;
  provided_file_system_->OpenFile(
      base::FilePath(kFilePath), OPEN_FILE_MODE_WRITE,
      base::BindOnce(LogOpenFile, base::Unretained(&log)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, log[0].second);

  const OpenedFiles& opened_files = provided_file_system_->GetOpenedFiles();
  EXPECT_EQ(0u, opened_files.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, OpenedFile_ClosingFailure) {
  OpenFileLog log;
  provided_file_system_->OpenFile(
      base::FilePath(kFilePath), OPEN_FILE_MODE_WRITE,
      base::BindOnce(LogOpenFile, base::Unretained(&log)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_OK, log[0].second);
  const int file_handle = log[0].first;

  const OpenedFiles& opened_files = provided_file_system_->GetOpenedFiles();
  const auto opened_file_it = opened_files.find(file_handle);
  ASSERT_NE(opened_files.end(), opened_file_it);
  EXPECT_EQ(kFilePath, opened_file_it->second.file_path.AsUTF8Unsafe());
  EXPECT_EQ(OPEN_FILE_MODE_WRITE, opened_file_it->second.mode);

  // Simulate an error for closing a file. Still, the file should be closed
  // in the C++ layer, anyway.
  event_router_->set_reply_result(base::File::FILE_ERROR_NOT_FOUND);

  Log close_log;
  provided_file_system_->CloseFile(
      file_handle, base::BindOnce(LogStatus, base::Unretained(&close_log)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, close_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, close_log[0]);
  EXPECT_EQ(0u, opened_files.size());
}

}  // namespace ash::file_system_provider
