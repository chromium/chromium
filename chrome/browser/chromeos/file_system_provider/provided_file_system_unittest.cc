// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/icon_set.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_observer.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/watcher.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/extension_id.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

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
      : EventRouter(profile, NULL),
        file_system_(file_system),
        reply_result_(base::File::FILE_OK) {}
  ~FakeEventRouter() override {}

  // Handles an event which would normally be routed to an extension. Instead
  // replies with a hard coded response.
  void DispatchEventToExtension(
      const extensions::ExtensionId& extension_id,
      std::unique_ptr<extensions::Event> event) override {
    ASSERT_TRUE(file_system_);
    std::string file_system_id;
    const base::DictionaryValue* dictionary_value = NULL;
    ASSERT_TRUE(event->event_args->GetDictionary(0, &dictionary_value));
    EXPECT_TRUE(dictionary_value->GetString("fileSystemId", &file_system_id));
    EXPECT_EQ(kFileSystemId, file_system_id);
    int request_id = -1;
    EXPECT_TRUE(dictionary_value->GetInteger("requestId", &request_id));
    EXPECT_TRUE(event->event_name == extensions::api::file_system_provider::
                                         OnAddWatcherRequested::kEventName ||
                event->event_name == extensions::api::file_system_provider::
                                         OnRemoveWatcherRequested::kEventName ||
                event->event_name == extensions::api::file_system_provider::
                                         OnOpenFileRequested::kEventName ||
                event->event_name == extensions::api::file_system_provider::
                                         OnCloseFileRequested::kEventName);

    if (reply_result_ == base::File::FILE_OK) {
      base::ListValue value_as_list;
      value_as_list.Set(0, std::make_unique<base::Value>(kFileSystemId));
      value_as_list.Set(1, std::make_unique<base::Value>(request_id));
      value_as_list.Set(2,
                        std::make_unique<base::Value>(0) /* execution_time */);

      using extensions::api::file_system_provider_internal::
          OperationRequestedSuccess::Params;
      std::unique_ptr<Params> params(Params::Create(value_as_list));
      ASSERT_TRUE(params.get());
      file_system_->GetRequestManager()->FulfillRequest(
          request_id,
          RequestValue::CreateForOperationSuccess(std::move(params)),
          false /* has_more */);
    } else {
      file_system_->GetRequestManager()->RejectRequest(
          request_id, std::make_unique<RequestValue>(), reply_result_);
    }
  }

  void set_reply_result(base::File::Error result) { reply_result_ = result; }

 private:
  ProvidedFileSystemInterface* const file_system_;  // Not owned.
  base::File::Error reply_result_;
  DISALLOW_COPY_AND_ASSIGN(FakeEventRouter);
};

// Observes the tested file system.
class Observer : public ProvidedFileSystemObserver {
 public:
  class ChangeEvent {
   public:
    ChangeEvent(storage::WatcherManager::ChangeType change_type,
                const ProvidedFileSystemObserver::Changes& changes)
        : change_type_(change_type), changes_(changes) {}
    virtual ~ChangeEvent() {}

    storage::WatcherManager::ChangeType change_type() const {
      return change_type_;
    }
    const ProvidedFileSystemObserver::Changes& changes() const {
      return changes_;
    }

   private:
    const storage::WatcherManager::ChangeType change_type_;
    const ProvidedFileSystemObserver::Changes changes_;

    DISALLOW_COPY_AND_ASSIGN(ChangeEvent);
  };

  Observer() : list_changed_counter_(0), tag_updated_counter_(0) {}

  // ProvidedFileSystemInterfaceObserver overrides.
  void OnWatcherChanged(const ProvidedFileSystemInfo& file_system_info,
                        const Watcher& watcher,
                        storage::WatcherManager::ChangeType change_type,
                        const ProvidedFileSystemObserver::Changes& changes,
                        const base::Closure& callback) override {
    EXPECT_EQ(kFileSystemId, file_system_info.file_system_id());
    change_events_.push_back(
        std::make_unique<ChangeEvent>(change_type, changes));
    complete_callback_ = callback;
  }

  void OnWatcherTagUpdated(const ProvidedFileSystemInfo& file_system_info,
                           const Watcher& watcher) override {
    EXPECT_EQ(kFileSystemId, file_system_info.file_system_id());
    ++tag_updated_counter_;
  }

  void OnWatcherListChanged(const ProvidedFileSystemInfo& file_system_info,
                            const Watchers& watchers) override {
    EXPECT_EQ(kFileSystemId, file_system_info.file_system_id());
    ++list_changed_counter_;
  }

  // Completes handling the OnWatcherChanged event.
  void CompleteOnWatcherChanged() {
    DCHECK(!complete_callback_.is_null());
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  complete_callback_);
    complete_callback_ = base::Closure();
  }

  int list_changed_counter() const { return list_changed_counter_; }
  const std::vector<std::unique_ptr<ChangeEvent>>& change_events() const {
    return change_events_;
  }
  int tag_updated_counter() const { return tag_updated_counter_; }

 private:
  std::vector<std::unique_ptr<ChangeEvent>> change_events_;
  int list_changed_counter_;
  int tag_updated_counter_;
  base::Closure complete_callback_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

// Stub notification manager, which works in unit tests.
class StubNotificationManager : public NotificationManagerInterface {
 public:
  StubNotificationManager() {}
  ~StubNotificationManager() override {}

  // NotificationManagerInterface overrides.
  void ShowUnresponsiveNotification(
      int id,
      const NotificationCallback& callback) override {}
  void HideUnresponsiveNotification(int id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StubNotificationManager);
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
                 base::File::Error result) {
  open_file_log->push_back(std::make_pair(file_handle, result));
}

}  // namespace

class FileSystemProviderProvidedFileSystemTest : public testing::Test {
 protected:
  FileSystemProviderProvidedFileSystemTest() {}
  ~FileSystemProviderProvidedFileSystemTest() override {}

  void SetUp() override {
    profile_.reset(new TestingProfile);
    const base::FilePath mount_path = util::GetMountPath(
        profile_.get(), ProviderId::CreateFromExtensionId(kExtensionId),
        kFileSystemId);
    MountOptions mount_options;
    mount_options.file_system_id = kFileSystemId;
    mount_options.display_name = kDisplayName;
    mount_options.supports_notify_tag = true;
    mount_options.writable = true;
    file_system_info_.reset(new ProvidedFileSystemInfo(
        kExtensionId, mount_options, mount_path, false /* configurable */,
        true /* watchable */, extensions::SOURCE_FILE, IconSet()));
    provided_file_system_.reset(
        new ProvidedFileSystem(profile_.get(), *file_system_info_.get()));
    event_router_.reset(
        new FakeEventRouter(profile_.get(), provided_file_system_.get()));
    event_router_->AddEventListener(extensions::api::file_system_provider::
                                        OnAddWatcherRequested::kEventName,
                                    NULL,
                                    kExtensionId);
    event_router_->AddEventListener(extensions::api::file_system_provider::
                                        OnRemoveWatcherRequested::kEventName,
                                    NULL,
                                    kExtensionId);
    event_router_->AddEventListener(
        extensions::api::file_system_provider::OnOpenFileRequested::kEventName,
        NULL, kExtensionId);
    event_router_->AddEventListener(
        extensions::api::file_system_provider::OnCloseFileRequested::kEventName,
        NULL, kExtensionId);
    provided_file_system_->SetEventRouterForTesting(event_router_.get());
    provided_file_system_->SetNotificationManagerForTesting(
        base::WrapUnique(new StubNotificationManager));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FakeEventRouter> event_router_;
  std::unique_ptr<ProvidedFileSystemInfo> file_system_info_;
  std::unique_ptr<ProvidedFileSystem> provided_file_system_;
};

TEST_F(FileSystemProviderProvidedFileSystemTest, AutoUpdater) {
  Log log;
  base::Closure firstCallback;
  base::Closure secondCallback;

  {
    // Auto updater is ref counted, and bound to all callbacks.
    scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(
        base::Bind(&LogStatus, base::Unretained(&log), base::File::FILE_OK)));

    firstCallback = auto_updater->CreateCallback();
    secondCallback = auto_updater->CreateCallback();
  }

  // Getting out of scope, should not invoke updating if there are pending
  // callbacks.
  EXPECT_EQ(0u, log.size());

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, firstCallback);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, log.size());

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, secondCallback);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, log.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AutoUpdater_NoCallbacks) {
  Log log;
  {
    scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(
        base::Bind(&LogStatus, base::Unretained(&log), base::File::FILE_OK)));
  }
  EXPECT_EQ(1u, log.size());
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AutoUpdater_CallbackIgnored) {
  Log log;
  {
    scoped_refptr<AutoUpdater> auto_updater(new AutoUpdater(
        base::Bind(&LogStatus, base::Unretained(&log), base::File::FILE_OK)));
    base::Closure callback = auto_updater->CreateCallback();
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
  Observer observer;

  provided_file_system_->AddObserver(&observer);

  // First, set the extension response to an error.
  event_router_->set_reply_result(base::File::FILE_ERROR_NOT_FOUND);

  provided_file_system_->AddWatcher(
      GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
      false /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
      base::Bind(&LogNotification, base::Unretained(&notification_log)));
  base::RunLoop().RunUntilIdle();

  // The directory should not become watched because of an error.
  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, log[0]);
  EXPECT_EQ(0u, notification_log.size());

  Watchers* const watchers = provided_file_system_->GetWatchers();
  EXPECT_EQ(0u, watchers->size());

  // The observer should not be called.
  EXPECT_EQ(0, observer.list_changed_counter());
  EXPECT_EQ(0, observer.tag_updated_counter());

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AddWatcher) {
  Log log;
  Observer observer;

  provided_file_system_->AddObserver(&observer);

  provided_file_system_->AddWatcher(
      GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
      true /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
      storage::WatcherManager::NotificationCallback());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_OK, log[0]);
  EXPECT_EQ(1, observer.list_changed_counter());
  EXPECT_EQ(0, observer.tag_updated_counter());

  Watchers* const watchers = provided_file_system_->GetWatchers();
  ASSERT_EQ(1u, watchers->size());
  const Watcher& watcher = watchers->begin()->second;
  EXPECT_EQ(kDirectoryPath, watcher.entry_path.value());
  EXPECT_FALSE(watcher.recursive);
  EXPECT_EQ("", watcher.last_tag);

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AddWatcher_PersistentIllegal) {
  {
    // Adding a persistent watcher with a notification callback is not allowed,
    // as it's basically impossible to restore the callback after a shutdown.
    Log log;
    NotificationLog notification_log;

    Observer observer;
    provided_file_system_->AddObserver(&observer);

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        true /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
        base::Bind(&LogNotification, base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, log[0]);
    EXPECT_EQ(0, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    provided_file_system_->RemoveObserver(&observer);
  }

  {
    // Adding a persistent watcher is not allowed if the file system doesn't
    // support the notify tag. It's because the notify tag is essential to be
    // able to recreate notification during shutdown.
    Log log;
    Observer observer;

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
        kExtensionId, mount_options, mount_path, false /* configurable */,
        true /* watchable */, extensions::SOURCE_FILE, IconSet());
    ProvidedFileSystem simple_provided_file_system(profile_.get(),
                                                   file_system_info);
    simple_provided_file_system.SetEventRouterForTesting(event_router_.get());
    simple_provided_file_system.SetNotificationManagerForTesting(
        base::WrapUnique(new StubNotificationManager));

    simple_provided_file_system.AddObserver(&observer);

    simple_provided_file_system.AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        true /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
        storage::WatcherManager::NotificationCallback());
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, log[0]);
    EXPECT_EQ(0, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    simple_provided_file_system.RemoveObserver(&observer);
  }
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AddWatcher_Exists) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);

  {
    // First watch a directory not recursively.
    Log log;
    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        true /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
        storage::WatcherManager::NotificationCallback());
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(1, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    ASSERT_TRUE(watchers);
    ASSERT_EQ(1u, watchers->size());
    const auto& watcher_it = watchers->find(
        WatcherKey(base::FilePath(kDirectoryPath), false /* recursive */));
    ASSERT_NE(watchers->end(), watcher_it);

    EXPECT_EQ(1u, watcher_it->second.subscribers.size());
    const auto& subscriber_it =
        watcher_it->second.subscribers.find(GURL(kOrigin));
    ASSERT_NE(watcher_it->second.subscribers.end(), subscriber_it);
    EXPECT_EQ(kOrigin, subscriber_it->second.origin.spec());
    EXPECT_TRUE(subscriber_it->second.persistent);
  }

  {
    // Create another non-recursive observer. That should fail.
    Log log;
    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        true /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
        storage::WatcherManager::NotificationCallback());
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_EXISTS, log[0]);
    EXPECT_EQ(1, observer.list_changed_counter());  // No changes on the list.
    EXPECT_EQ(0, observer.tag_updated_counter());
  }

  {
    // Lastly, create another recursive observer. That should succeed.
    Log log;
    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), true /* recursive */,
        true /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
        storage::WatcherManager::NotificationCallback());
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(2, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());
  }

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, AddWatcher_MultipleOrigins) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);

  {
    // First watch a directory not recursively.
    Log log;
    NotificationLog notification_log;

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        false /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
        base::Bind(&LogNotification, base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(1, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());
    EXPECT_EQ(0u, notification_log.size());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    ASSERT_TRUE(watchers);
    ASSERT_EQ(1u, watchers->size());
    const auto& watcher_it = watchers->find(
        WatcherKey(base::FilePath(kDirectoryPath), false /* recursive */));
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
    // Create another watcher, but recursive and with a different origin.
    Log log;
    NotificationLog notification_log;

    provided_file_system_->AddWatcher(
        GURL(kAnotherOrigin), base::FilePath(kDirectoryPath),
        true /* recursive */, false /* persistent */,
        base::Bind(&LogStatus, base::Unretained(&log)),
        base::Bind(&LogNotification, base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(2, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());
    EXPECT_EQ(0u, notification_log.size());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    ASSERT_TRUE(watchers);
    ASSERT_EQ(2u, watchers->size());
    const auto& watcher_it = watchers->find(
        WatcherKey(base::FilePath(kDirectoryPath), false /* recursive */));
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
    // Remove the second watcher gracefully.
    Log log;
    provided_file_system_->RemoveWatcher(
        GURL(kAnotherOrigin), base::FilePath(kDirectoryPath),
        true /* recursive */, base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(3, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    ASSERT_TRUE(watchers);
    EXPECT_EQ(1u, watchers->size());
    const auto& watcher_it = watchers->find(
        WatcherKey(base::FilePath(kDirectoryPath), false /* recursive */));
    ASSERT_NE(watchers->end(), watcher_it);

    EXPECT_EQ(1u, watcher_it->second.subscribers.size());
    const auto& subscriber_it =
        watcher_it->second.subscribers.find(GURL(kOrigin));
    ASSERT_NE(watcher_it->second.subscribers.end(), subscriber_it);
    EXPECT_EQ(kOrigin, subscriber_it->first.spec());
    EXPECT_EQ(kOrigin, subscriber_it->second.origin.spec());
    EXPECT_FALSE(subscriber_it->second.persistent);
  }

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, RemoveWatcher) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);

  {
    // First, confirm that removing a watcher which does not exist results in an
    // error.
    Log log;
    provided_file_system_->RemoveWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, log[0]);
    EXPECT_EQ(0, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());
  }

  {
    // Watch a directory not recursively.
    Log log;
    NotificationLog notification_log;

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        false /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
        base::Bind(&LogNotification, base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(1, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());
    EXPECT_EQ(0u, notification_log.size());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(1u, watchers->size());
  }

  {
    // Remove a watcher gracefully.
    Log log;
    provided_file_system_->RemoveWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(2, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(0u, watchers->size());
  }

  {
    // Confirm that it's possible to watch it again.
    Log log;
    NotificationLog notification_log;

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        false /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
        base::Bind(&LogNotification, base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(3, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());
    EXPECT_EQ(0u, notification_log.size());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(1u, watchers->size());
  }

  {
    // Finally, remove it, but with an error from extension. That should result
    // in a removed watcher, anyway. The error code should not be passed.
    event_router_->set_reply_result(base::File::FILE_ERROR_FAILED);

    Log log;
    provided_file_system_->RemoveWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(4, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());

    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(0u, watchers->size());
  }

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, Notify) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);
  NotificationLog notification_log;

  {
    // Watch a directory.
    Log log;

    provided_file_system_->AddWatcher(
        GURL(kOrigin), base::FilePath(kDirectoryPath), false /* recursive */,
        false /* persistent */, base::Bind(&LogStatus, base::Unretained(&log)),
        base::Bind(&LogNotification, base::Unretained(&notification_log)));
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);
    EXPECT_EQ(1, observer.list_changed_counter());
    EXPECT_EQ(0, observer.tag_updated_counter());
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

    Log log;
    provided_file_system_->Notify(
        base::FilePath(kDirectoryPath), false /* recursive */, change_type,
        base::WrapUnique(new ProvidedFileSystemObserver::Changes), tag,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    // Confirm that the notification callback was called.
    ASSERT_EQ(1u, notification_log.size());
    EXPECT_EQ(change_type, notification_log[0]);

    // Verify the observer event.
    ASSERT_EQ(1u, observer.change_events().size());
    const Observer::ChangeEvent* const change_event =
        observer.change_events()[0].get();
    EXPECT_EQ(change_type, change_event->change_type());
    EXPECT_EQ(0u, change_event->changes().size());

    // The tag should not be updated in advance, before all observers handle
    // the notification.
    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(1u, watchers->size());
    provided_file_system_->GetWatchers();
    EXPECT_EQ("", watchers->begin()->second.last_tag);

    // Wait until all observers finish handling the notification.
    observer.CompleteOnWatcherChanged();
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);

    // Confirm, that the watcher still exists, and that the tag is updated.
    ASSERT_EQ(1u, watchers->size());
    EXPECT_EQ(tag, watchers->begin()->second.last_tag);
    EXPECT_EQ(1, observer.list_changed_counter());
    EXPECT_EQ(1, observer.tag_updated_counter());
  }

  {
    // Notify about deleting of the watched entry.
    const storage::WatcherManager::ChangeType change_type =
        storage::WatcherManager::DELETED;
    const ProvidedFileSystemObserver::Changes changes;
    const std::string tag = "chocolate-disco";

    Log log;
    provided_file_system_->Notify(
        base::FilePath(kDirectoryPath), false /* recursive */, change_type,
        base::WrapUnique(new ProvidedFileSystemObserver::Changes), tag,
        base::Bind(&LogStatus, base::Unretained(&log)));
    base::RunLoop().RunUntilIdle();

    // Complete all change events.
    observer.CompleteOnWatcherChanged();
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(1u, log.size());
    EXPECT_EQ(base::File::FILE_OK, log[0]);

    // Confirm that the notification callback was called.
    ASSERT_EQ(2u, notification_log.size());
    EXPECT_EQ(change_type, notification_log[1]);

    // Verify the observer event.
    ASSERT_EQ(2u, observer.change_events().size());
    const Observer::ChangeEvent* const change_event =
        observer.change_events()[1].get();
    EXPECT_EQ(change_type, change_event->change_type());
    EXPECT_EQ(0u, change_event->changes().size());
  }

  // Confirm, that the watcher is removed.
  {
    Watchers* const watchers = provided_file_system_->GetWatchers();
    EXPECT_EQ(0u, watchers->size());
    EXPECT_EQ(2, observer.list_changed_counter());
    EXPECT_EQ(2, observer.tag_updated_counter());
  }

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, OpenedFiles) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);

  OpenFileLog log;
  provided_file_system_->OpenFile(
      base::FilePath(kFilePath), OPEN_FILE_MODE_WRITE,
      base::Bind(LogOpenFile, base::Unretained(&log)));
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
      file_handle, base::Bind(LogStatus, base::Unretained(&close_log)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, close_log.size());
  EXPECT_EQ(base::File::FILE_OK, close_log[0]);
  EXPECT_EQ(0u, opened_files.size());

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, OpenedFiles_OpeningFailure) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);

  event_router_->set_reply_result(base::File::FILE_ERROR_NOT_FOUND);

  OpenFileLog log;
  provided_file_system_->OpenFile(
      base::FilePath(kFilePath), OPEN_FILE_MODE_WRITE,
      base::Bind(LogOpenFile, base::Unretained(&log)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, log.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, log[0].second);

  const OpenedFiles& opened_files = provided_file_system_->GetOpenedFiles();
  EXPECT_EQ(0u, opened_files.size());

  provided_file_system_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderProvidedFileSystemTest, OpenedFile_ClosingFailure) {
  Observer observer;
  provided_file_system_->AddObserver(&observer);

  OpenFileLog log;
  provided_file_system_->OpenFile(
      base::FilePath(kFilePath), OPEN_FILE_MODE_WRITE,
      base::Bind(LogOpenFile, base::Unretained(&log)));
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
      file_handle, base::Bind(LogStatus, base::Unretained(&close_log)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, close_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, close_log[0]);
  EXPECT_EQ(0u, opened_files.size());

  provided_file_system_->RemoveObserver(&observer);
}

}  // namespace file_system_provider
}  // namespace chromeos
