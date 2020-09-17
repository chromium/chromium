// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/drive/drivefs_test_support.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_observer.h"
#include "chrome/browser/chromeos/file_manager/mount_test_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/smb_client/smb_service.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_provider.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/drivefs/drivefs_host.h"
#include "chromeos/components/drivefs/fake_drivefs.h"
#include "chromeos/components/smbfs/smbfs_host.h"
#include "chromeos/components/smbfs/smbfs_mounter.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "chromeos/disks/mount_point.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/api/test.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

using ::testing::_;

class SelectFileDialogExtensionTestFactory
    : public ui::SelectFileDialogFactory {
 public:
  SelectFileDialogExtensionTestFactory() = default;
  ~SelectFileDialogExtensionTestFactory() override = default;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    last_select_ =
        SelectFileDialogExtension::Create(listener, std::move(policy));
    return last_select_.get();
  }

  views::Widget* GetLastWidget() {
    return last_select_->extension_dialog_->GetWidget();
  }

 private:
  scoped_refptr<SelectFileDialogExtension> last_select_;
};

namespace file_manager {
namespace {

// During test, the test extensions can send a list of entries (directories
// or files) to add to a target volume using an AddEntriesMessage command.
//
// During a files app browser test, the "addEntries" message (see onCommand()
// below when name is "addEntries"). This adds them to the fake file system that
// is being used for testing.
//
// Here, we define some useful types to help parse the JSON from the addEntries
// format. The RegisterJSONConverter() method defines the expected types of each
// field from the message and which member variables to save them in.
//
// The "addEntries" message contains a vector of TestEntryInfo, which contains
// various nested subtypes:
//
//   * EntryType, which represents the type of entry (defined as an enum and
//     converted from the JSON string representation in MapStringToEntryType)
//
//   * SharedOption, representing whether the file is shared and appears in the
//     Shared with Me section of the app (similarly converted from the JSON
//     string representation to an enum for storing in MapStringToSharedOption)
//
//   * EntryCapabilities, which represents the capabilities (permissions) for
//     the new entry
//
//   * TestEntryInfo, which stores all of the above information, plus more
//     metadata about the entry.
//
// AddEntriesMessage contains an array of TestEntryInfo (one for each entry to
// add), plus the volume to add the entries to. It is constructed from JSON-
// parseable format as described in RegisterJSONConverter.
struct AddEntriesMessage {
  // Utility types.
  struct EntryCapabilities;
  struct TestEntryInfo;

  // Represents the various volumes available for adding entries.
  enum TargetVolume {
    LOCAL_VOLUME,
    DRIVE_VOLUME,
    CROSTINI_VOLUME,
    USB_VOLUME,
    ANDROID_FILES_VOLUME,
    DOCUMENTS_PROVIDER_VOLUME,
    MEDIA_VIEW_AUDIO,
    MEDIA_VIEW_IMAGES,
    MEDIA_VIEW_VIDEOS,
    SMBFS_VOLUME,
  };

  // Represents the different types of entries (e.g. file, folder).
  enum EntryType { FILE, DIRECTORY, LINK, TEAM_DRIVE, COMPUTER };

  // Represents whether an entry appears in 'Share with Me' or not.
  enum SharedOption { NONE, SHARED, SHARED_WITH_ME, NESTED_SHARED_WITH_ME };

  // The actual AddEntriesMessage contents.

  // The volume to add |entries| to.
  TargetVolume volume;

  // The |entries| to be added.
  std::vector<std::unique_ptr<struct TestEntryInfo>> entries;

  // Converts |value| to an AddEntriesMessage: true on success.
  static bool ConvertJSONValue(const base::DictionaryValue& value,
                               AddEntriesMessage* message) {
    base::JSONValueConverter<AddEntriesMessage> converter;
    return converter.Convert(value, message);
  }

  // Registers AddEntriesMessage member info to the |converter|.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AddEntriesMessage>* converter) {
    converter->RegisterCustomField("volume", &AddEntriesMessage::volume,
                                   &MapStringToTargetVolume);
    converter->RegisterRepeatedMessage<struct TestEntryInfo>(
        "entries", &AddEntriesMessage::entries);
  }

  // Maps |value| to TargetVolume. Returns true on success.
  static bool MapStringToTargetVolume(base::StringPiece value,
                                      TargetVolume* volume) {
    if (value == "local")
      *volume = LOCAL_VOLUME;
    else if (value == "drive")
      *volume = DRIVE_VOLUME;
    else if (value == "crostini")
      *volume = CROSTINI_VOLUME;
    else if (value == "usb")
      *volume = USB_VOLUME;
    else if (value == "android_files")
      *volume = ANDROID_FILES_VOLUME;
    else if (value == "documents_provider")
      *volume = DOCUMENTS_PROVIDER_VOLUME;
    else if (value == "media_view_audio")
      *volume = MEDIA_VIEW_AUDIO;
    else if (value == "media_view_images")
      *volume = MEDIA_VIEW_IMAGES;
    else if (value == "media_view_videos")
      *volume = MEDIA_VIEW_VIDEOS;
    else if (value == "smbfs")
      *volume = SMBFS_VOLUME;
    else
      return false;
    return true;
  }

  // A message that specifies the capabilities (permissions) for the entry, in
  // a dictionary in JSON-parseable format.
  struct EntryCapabilities {
    EntryCapabilities()
        : can_copy(true),
          can_delete(true),
          can_rename(true),
          can_add_children(true),
          can_share(true) {}

    EntryCapabilities(bool can_copy,
                      bool can_delete,
                      bool can_rename,
                      bool can_add_children,
                      bool can_share)
        : can_copy(can_copy),
          can_delete(can_delete),
          can_rename(can_rename),
          can_add_children(can_add_children),
          can_share(can_share) {}

    bool can_copy;    // Whether the user can copy this file or directory.
    bool can_delete;  // Whether the user can delete this file or directory.
    bool can_rename;  // Whether the user can rename this file or directory.
    bool can_add_children;  // For directories, whether the user can add
                            // children to this directory.
    bool can_share;  // Whether the user can share this file or directory.

    static void RegisterJSONConverter(
        base::JSONValueConverter<EntryCapabilities>* converter) {
      converter->RegisterBoolField("canCopy", &EntryCapabilities::can_copy);
      converter->RegisterBoolField("canDelete", &EntryCapabilities::can_delete);
      converter->RegisterBoolField("canRename", &EntryCapabilities::can_rename);
      converter->RegisterBoolField("canAddChildren",
                                   &EntryCapabilities::can_add_children);
      converter->RegisterBoolField("canShare", &EntryCapabilities::can_share);
    }
  };

  // A message that specifies the folder features for the entry, in a
  // dictionary in JSON-parseable format.
  struct EntryFolderFeature {
    EntryFolderFeature()
        : is_machine_root(false),
          is_arbitrary_sync_folder(false),
          is_external_media(false) {}

    EntryFolderFeature(bool is_machine_root,
                       bool is_arbitrary_sync_folder,
                       bool is_external_media)
        : is_machine_root(is_machine_root),
          is_arbitrary_sync_folder(is_arbitrary_sync_folder),
          is_external_media(is_external_media) {}

    bool is_machine_root;           // Is a root entry in the Computers section.
    bool is_arbitrary_sync_folder;  // True if this is a sync folder for
                                    // backup and sync.
    bool is_external_media;         // True is this is a root entry for a
                                    // removable devices (USB, SD etc).

    static void RegisterJSONConverter(
        base::JSONValueConverter<EntryFolderFeature>* converter) {
      converter->RegisterBoolField("isMachineRoot",
                                   &EntryFolderFeature::is_machine_root);
      converter->RegisterBoolField(
          "isArbitrarySyncFolder",
          &EntryFolderFeature::is_arbitrary_sync_folder);
      converter->RegisterBoolField("isExternalMedia",
                                   &EntryFolderFeature::is_external_media);
    }
  };

  // A message that specifies the metadata (name, shared options, capabilities
  // etc) for an entry, in a dictionary in JSON-parseable format.
  // This object must match TestEntryInfo in
  // ui/file_manager/integration_tests/test_util.js, which generates the message
  // that contains this object.
  struct TestEntryInfo {
    TestEntryInfo() : type(FILE), shared_option(NONE) {}

    TestEntryInfo(EntryType type,
                  const std::string& source_file_name,
                  const std::string& target_path)
        : type(type),
          shared_option(NONE),
          source_file_name(source_file_name),
          target_path(target_path),
          last_modified_time(base::Time::Now()) {}

    EntryType type;                  // Entry type: file or directory.
    SharedOption shared_option;      // File entry sharing option.
    std::string source_file_name;    // Source file name prototype.
    std::string target_path;         // Target file or directory path.
    std::string name_text;           // Display file name.
    std::string team_drive_name;     // Name of team drive this entry is in.
    std::string computer_name;       // Name of the computer this entry is in.
    std::string mime_type;           // File entry content mime type.
    base::Time last_modified_time;   // Entry last modified time.
    EntryCapabilities capabilities;  // Entry permissions.
    EntryFolderFeature folder_feature;  // Entry folder feature.
    bool pinned = false;                // Whether the file should be pinned.

    TestEntryInfo& SetSharedOption(SharedOption option) {
      shared_option = option;
      return *this;
    }

    TestEntryInfo& SetMimeType(const std::string& type) {
      mime_type = type;
      return *this;
    }

    TestEntryInfo& SetTeamDriveName(const std::string& name) {
      team_drive_name = name;
      return *this;
    }

    TestEntryInfo& SetComputerName(const std::string& name) {
      computer_name = name;
      return *this;
    }

    TestEntryInfo& SetLastModifiedTime(const base::Time& time) {
      last_modified_time = time;
      return *this;
    }

    TestEntryInfo& SetEntryCapabilities(
        const EntryCapabilities& new_capabilities) {
      capabilities = new_capabilities;
      return *this;
    }

    TestEntryInfo& SetEntryFolderFeature(
        const EntryFolderFeature& new_folder_feature) {
      folder_feature = new_folder_feature;
      return *this;
    }

    TestEntryInfo& SetPinned(bool is_pinned) {
      pinned = is_pinned;
      return *this;
    }

    // Registers the member information to the given converter.
    static void RegisterJSONConverter(
        base::JSONValueConverter<TestEntryInfo>* converter) {
      converter->RegisterCustomField("type", &TestEntryInfo::type,
                                     &MapStringToEntryType);
      converter->RegisterStringField("sourceFileName",
                                     &TestEntryInfo::source_file_name);
      converter->RegisterStringField("targetPath", &TestEntryInfo::target_path);
      converter->RegisterStringField("nameText", &TestEntryInfo::name_text);
      converter->RegisterStringField("teamDriveName",
                                     &TestEntryInfo::team_drive_name);
      converter->RegisterStringField("computerName",
                                     &TestEntryInfo::computer_name);
      converter->RegisterStringField("mimeType", &TestEntryInfo::mime_type);
      converter->RegisterCustomField("sharedOption",
                                     &TestEntryInfo::shared_option,
                                     &MapStringToSharedOption);
      converter->RegisterCustomField("lastModifiedTime",
                                     &TestEntryInfo::last_modified_time,
                                     &MapStringToTime);
      converter->RegisterNestedField("capabilities",
                                     &TestEntryInfo::capabilities);
      converter->RegisterNestedField("folderFeature",
                                     &TestEntryInfo::folder_feature);
      converter->RegisterBoolField("pinned", &TestEntryInfo::pinned);
    }

    // Maps |value| to an EntryType. Returns true on success.
    static bool MapStringToEntryType(base::StringPiece value, EntryType* type) {
      if (value == "file")
        *type = FILE;
      else if (value == "directory")
        *type = DIRECTORY;
      else if (value == "link")
        *type = LINK;
      else if (value == "team_drive")
        *type = TEAM_DRIVE;
      else if (value == "Computer")
        *type = COMPUTER;
      else
        return false;
      return true;
    }

    // Maps |value| to SharedOption. Returns true on success.
    static bool MapStringToSharedOption(base::StringPiece value,
                                        SharedOption* option) {
      if (value == "shared")
        *option = SHARED;
      else if (value == "sharedWithMe")
        *option = SHARED_WITH_ME;
      else if (value == "nestedSharedWithMe")
        *option = NESTED_SHARED_WITH_ME;
      else if (value == "none")
        *option = NONE;
      else
        return false;
      return true;
    }

    // Maps |value| to base::Time. Returns true on success.
    static bool MapStringToTime(base::StringPiece value, base::Time* time) {
      return base::Time::FromString(value.as_string().c_str(), time);
    }
  };
};

// Listens for chrome.test messages: PASS, FAIL, and SendMessage.
class FileManagerTestMessageListener : public content::NotificationObserver {
 public:
  struct Message {
    int type;
    std::string message;
    scoped_refptr<extensions::TestSendMessageFunction> function;
  };

  FileManagerTestMessageListener() {
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_TEST_PASSED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_TEST_FAILED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  Message GetNextMessage() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (messages_.empty()) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    DCHECK(!messages_.empty());
    const Message next = messages_.front();
    messages_.pop_front();
    return next;
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    Message message{type, std::string(), nullptr};
    if (type == extensions::NOTIFICATION_EXTENSION_TEST_PASSED) {
      test_complete_ = true;
    } else if (type == extensions::NOTIFICATION_EXTENSION_TEST_FAILED) {
      message.message = *content::Details<std::string>(details).ptr();
      test_complete_ = true;
    } else if (type == extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE) {
      message.message = *content::Details<std::string>(details).ptr();
      using SendMessage = content::Source<extensions::TestSendMessageFunction>;
      message.function = SendMessage(source).ptr();
      using WillReply = content::Details<std::pair<std::string, bool*>>;
      *WillReply(details).ptr()->second = true;  // crbug.com/668680
      CHECK(!test_complete_) << "LATE MESSAGE: " << message.message;
    }

    messages_.push_back(message);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  bool test_complete_ = false;
  base::OnceClosure quit_closure_;
  base::circular_deque<Message> messages_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(FileManagerTestMessageListener);
};

// Test volume.
class TestVolume {
 protected:
  explicit TestVolume(const std::string& name) : name_(name) {}
  virtual ~TestVolume() = default;

  bool CreateRootDirectory(const Profile* profile) {
    if (root_initialized_)
      return true;
    root_ = profile->GetPath().Append(name_);
    base::ScopedAllowBlockingForTesting allow_blocking;
    root_initialized_ = base::CreateDirectory(root_);
    return root_initialized_;
  }

  const std::string& name() const { return name_; }
  const base::FilePath& root_path() const { return root_; }

  static base::FilePath GetTestDataFilePath(const std::string& file_name) {
    // Get the path to file manager's test data directory.
    base::FilePath source_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
    auto test_data_dir = source_dir.AppendASCII("chrome")
                             .AppendASCII("test")
                             .AppendASCII("data")
                             .AppendASCII("chromeos")
                             .AppendASCII("file_manager");
    // Return full test data path to the given |file_name|.
    return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
  }

 private:
  base::FilePath root_;
  bool root_initialized_ = false;
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(TestVolume);
};

base::Lock& GetLockForBlockingDefaultFileTaskRunner() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// Ensures the default HTML filesystem API blocking task runner is blocked for a
// test.
void BlockFileTaskRunner(Profile* profile)
    EXCLUSIVE_LOCK_FUNCTION(GetLockForBlockingDefaultFileTaskRunner()) {
  GetLockForBlockingDefaultFileTaskRunner().Acquire();

  content::BrowserContext::GetDefaultStoragePartition(profile)
      ->GetFileSystemContext()
      ->default_file_task_runner()
      ->PostTask(FROM_HERE, base::BindOnce([] {
                   base::AutoLock l(GetLockForBlockingDefaultFileTaskRunner());
                 }));
}

// Undo the effects of |BlockFileTaskRunner()|.
void UnblockFileTaskRunner()
    UNLOCK_FUNCTION(GetLockForBlockingDefaultFileTaskRunner()) {
  GetLockForBlockingDefaultFileTaskRunner().Release();
}

struct ExpectFileTasksMessage {
  static bool ConvertJSONValue(const base::DictionaryValue& value,
                               ExpectFileTasksMessage* message) {
    base::JSONValueConverter<ExpectFileTasksMessage> converter;
    return converter.Convert(value, message);
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<ExpectFileTasksMessage>* converter) {
    converter->RegisterCustomField(
        "openType", &ExpectFileTasksMessage::open_type, &MapStringToOpenType);
    converter->RegisterRepeatedString("fileNames",
                                      &ExpectFileTasksMessage::file_names);
  }

  static bool MapStringToOpenType(
      base::StringPiece value,
      file_tasks::FileTasksObserver::OpenType* open_type) {
    using OpenType = file_tasks::FileTasksObserver::OpenType;
    if (value == "launch") {
      *open_type = OpenType::kLaunch;
    } else if (value == "open") {
      *open_type = OpenType::kOpen;
    } else if (value == "saveAs") {
      *open_type = OpenType::kSaveAs;
    } else if (value == "download") {
      *open_type = OpenType::kDownload;
    } else {
      return false;
    }
    return true;
  }

  std::vector<std::unique_ptr<std::string>> file_names;
  file_tasks::FileTasksObserver::OpenType open_type;
};

struct GetHistogramCountMessage {
  static bool ConvertJSONValue(const base::DictionaryValue& value,
                               GetHistogramCountMessage* message) {
    base::JSONValueConverter<GetHistogramCountMessage> converter;
    return converter.Convert(value, message);
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<GetHistogramCountMessage>* converter) {
    converter->RegisterStringField("histogramName",
                                   &GetHistogramCountMessage::histogram_name);
    converter->RegisterIntField("value", &GetHistogramCountMessage::value);
  }

  std::string histogram_name;
  int value = 0;
};

struct GetUserActionCountMessage {
  static bool ConvertJSONValue(const base::DictionaryValue& value,
                               GetUserActionCountMessage* message) {
    base::JSONValueConverter<GetUserActionCountMessage> converter;
    return converter.Convert(value, message);
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<GetUserActionCountMessage>* converter) {
    converter->RegisterStringField(
        "userActionName", &GetUserActionCountMessage::user_action_name);
  }

  std::string user_action_name;
};

struct GetLocalPathMessage {
  static bool ConvertJSONValue(const base::DictionaryValue& value,
                               GetLocalPathMessage* message) {
    base::JSONValueConverter<GetLocalPathMessage> converter;
    return converter.Convert(value, message);
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<GetLocalPathMessage>* converter) {
    converter->RegisterStringField("localPath",
                                   &GetLocalPathMessage::local_path);
  }

  std::string local_path;
};

}  // anonymous namespace

std::ostream& operator<<(std::ostream& out, const GuestMode mode) {
  switch (mode) {
    case NOT_IN_GUEST_MODE:
      return out << "normal";
    case IN_GUEST_MODE:
      return out << "guest";
    case IN_INCOGNITO:
      return out << "incognito";
  }
}

FileManagerBrowserTestBase::Options::Options() = default;
FileManagerBrowserTestBase::Options::Options(const Options&) = default;

std::ostream& operator<<(std::ostream& out,
                         const FileManagerBrowserTestBase::Options& options) {
  out << "{";

  // Don't print separator before first member.
  auto sep = [i = 0]() mutable { return i++ ? ", " : ""; };

  // Only print members with non-default values.
  const FileManagerBrowserTestBase::Options defaults;

  // Print guest mode first, followed by boolean members in lexicographic order.
  if (options.guest_mode != defaults.guest_mode)
    out << sep() << options.guest_mode;

#define PRINT_IF_NOT_DEFAULT(N) \
  if (options.N != defaults.N)  \
    out << sep() << (options.N ? "" : "!") << #N;

  PRINT_IF_NOT_DEFAULT(arc)
  PRINT_IF_NOT_DEFAULT(browser)
  PRINT_IF_NOT_DEFAULT(documents_provider)
  PRINT_IF_NOT_DEFAULT(files_ng)
  PRINT_IF_NOT_DEFAULT(mount_volumes)
  PRINT_IF_NOT_DEFAULT(native_smb)
  PRINT_IF_NOT_DEFAULT(offline)
  PRINT_IF_NOT_DEFAULT(smbfs)
  PRINT_IF_NOT_DEFAULT(tablet_mode)
  PRINT_IF_NOT_DEFAULT(zip)
  PRINT_IF_NOT_DEFAULT(zip_no_nacl)

#undef PRINT_IF_NOT_DEFAULT

  return out << "}";
}

class FileManagerBrowserTestBase::MockFileTasksObserver
    : public file_tasks::FileTasksObserver {
 public:
  explicit MockFileTasksObserver(Profile* profile) : observer_(this) {
    observer_.Add(file_tasks::FileTasksNotifier::GetForProfile(profile));
  }

  MOCK_METHOD2(OnFilesOpenedImpl,
               void(const std::string& path, OpenType open_type));

  void OnFilesOpened(const std::vector<FileOpenEvent>& opens) {
    ASSERT_TRUE(!opens.empty());
    for (auto& open : opens) {
      OnFilesOpenedImpl(open.path.value(), open.open_type);
    }
  }

 private:
  ScopedObserver<file_tasks::FileTasksNotifier, file_tasks::FileTasksObserver>
      observer_;
};

// LocalTestVolume: test volume for a local drive.
class LocalTestVolume : public TestVolume {
 public:
  explicit LocalTestVolume(const std::string& name) : TestVolume(name) {}
  ~LocalTestVolume() override = default;

  // Adds this local volume. Returns true on success.
  virtual bool Mount(Profile* profile) = 0;

  virtual void CreateEntry(const AddEntriesMessage::TestEntryInfo& entry) {
    CreateEntryImpl(entry, root_path().AppendASCII(entry.target_path));
  }

  void InsertEntryOnMap(const AddEntriesMessage::TestEntryInfo& entry,
                        const base::FilePath& target_path) {
    const auto it = entries_.find(target_path);
    if (it == entries_.end())
      entries_.insert(std::make_pair(target_path, entry));
  }

  void CreateEntryImpl(const AddEntriesMessage::TestEntryInfo& entry,
                       const base::FilePath& target_path) {
    entries_.insert(std::make_pair(target_path, entry));
    switch (entry.type) {
      case AddEntriesMessage::FILE: {
        const base::FilePath source_path =
            TestVolume::GetTestDataFilePath(entry.source_file_name);
        ASSERT_TRUE(base::CopyFile(source_path, target_path))
            << "Copy from " << source_path.value() << " to "
            << target_path.value() << " failed.";
        break;
      }
      case AddEntriesMessage::DIRECTORY:
        ASSERT_TRUE(base::CreateDirectory(target_path))
            << "Failed to create a directory: " << target_path.value();
        break;
      case AddEntriesMessage::LINK:
        ASSERT_TRUE(base::CreateSymbolicLink(
            base::FilePath(entry.source_file_name), target_path))
            << "Failed to create a symlink: " << target_path.value();
        break;
      case AddEntriesMessage::TEAM_DRIVE:
        NOTREACHED() << "Can't create a team drive in a local volume: "
                     << target_path.value();
        break;
      case AddEntriesMessage::COMPUTER:
        NOTREACHED() << "Can't create a computer in a local volume: "
                     << target_path.value();
        break;
      default:
        NOTREACHED() << "Unsupported entry type for: " << target_path.value();
    }

    ASSERT_TRUE(UpdateModifiedTime(entry, target_path));
  }

 private:
  // Updates the ModifiedTime of the entry, and its parent directories if
  // needed. Returns true on success.
  bool UpdateModifiedTime(const AddEntriesMessage::TestEntryInfo& entry,
                          const base::FilePath& path) {
    if (!base::TouchFile(path, entry.last_modified_time,
                         entry.last_modified_time)) {
      return false;
    }

    // Update the modified time of parent directories because they may be
    // also affected by the update of child items.
    if (path.DirName() != root_path()) {
      const auto& it = entries_.find(path.DirName());
      if (it == entries_.end())
        return false;
      return UpdateModifiedTime(it->second, path.DirName());
    }

    return true;
  }

  std::map<base::FilePath, const AddEntriesMessage::TestEntryInfo> entries_;

  DISALLOW_COPY_AND_ASSIGN(LocalTestVolume);
};

// DownloadsTestVolume: local test volume for the "Downloads" directory.
class DownloadsTestVolume : public LocalTestVolume {
 public:
  DownloadsTestVolume() : LocalTestVolume("MyFiles") {}
  ~DownloadsTestVolume() override = default;

  void EnsureDownloadsFolderExists() {
    // When MyFiles is the volume create the Downloads folder under it.
    auto downloads_folder = root_path().Append("Downloads");
    auto downloads_entry = AddEntriesMessage::TestEntryInfo(
        AddEntriesMessage::DIRECTORY, "", "Downloads");
    if (!base::PathExists(downloads_folder))
      CreateEntryImpl(downloads_entry, downloads_folder);

    // Make sure that Downloads exists in the local entries_ map, in case the
    // folder in the FS has been created by a PRE_ routine.
    InsertEntryOnMap(downloads_entry, downloads_folder);
  }
  // Forces the content to be created inside MyFiles/Downloads when MyFiles is
  // the Volume, so tests are compatible with volume being MyFiles or Downloads.
  // TODO(lucmult): Remove this special case once MyFiles volume has been
  // rolled out.
  base::FilePath base_path() const { return root_path().Append("Downloads"); }

  bool Mount(Profile* profile) override {
    if (!CreateRootDirectory(profile))
      return false;
    EnsureDownloadsFolderExists();
    auto* volume = VolumeManager::Get(profile);
    return volume->RegisterDownloadsDirectoryForTesting(root_path());
  }

  void CreateEntry(const AddEntriesMessage::TestEntryInfo& entry) override {
    base::FilePath target_path = base_path().Append(entry.target_path);
    CreateEntryImpl(entry, target_path);
  }

  void Unmount(Profile* profile) {
    auto* volume = VolumeManager::Get(profile);
    volume->RemoveDownloadsDirectoryForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsTestVolume);
};

class AndroidFilesTestVolume : public LocalTestVolume {
 public:
  AndroidFilesTestVolume() : LocalTestVolume("AndroidFiles") {}
  ~AndroidFilesTestVolume() override = default;

  bool Mount(Profile* profile) override {
    return CreateRootDirectory(profile) &&
           VolumeManager::Get(profile)->RegisterAndroidFilesDirectoryForTesting(
               root_path());
  }

  const base::FilePath& mount_path() const { return root_path(); }

  void Unmount(Profile* profile) {
    VolumeManager::Get(profile)->RemoveAndroidFilesDirectoryForTesting(
        root_path());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidFilesTestVolume);
};

// CrostiniTestVolume: local test volume for the "Linux files" directory.
class CrostiniTestVolume : public LocalTestVolume {
 public:
  CrostiniTestVolume() : LocalTestVolume("Crostini") {}
  ~CrostiniTestVolume() override = default;

  // Create root dir so entries can be created, but volume is not mounted.
  bool Initialize(Profile* profile) { return CreateRootDirectory(profile); }

  bool Mount(Profile* profile) override {
    return CreateRootDirectory(profile) &&
           VolumeManager::Get(profile)->RegisterCrostiniDirectoryForTesting(
               root_path());
  }

  const base::FilePath& mount_path() const { return root_path(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniTestVolume);
};

// FakeTestVolume: local test volume with a given volume and device type.
class FakeTestVolume : public LocalTestVolume {
 public:
  FakeTestVolume(const std::string& name,
                 VolumeType volume_type,
                 chromeos::DeviceType device_type)
      : LocalTestVolume(name),
        volume_type_(volume_type),
        device_type_(device_type) {}
  ~FakeTestVolume() override = default;

  // Add the fake test volume entries.
  bool PrepareTestEntries(Profile* profile) {
    if (!CreateRootDirectory(profile))
      return false;

    // Note: must be kept in sync with BASIC_FAKE_ENTRY_SET defined in the
    // integration_tests/file_manager JS code.
    CreateEntry(AddEntriesMessage::TestEntryInfo(AddEntriesMessage::FILE,
                                                 "text.txt", "hello.txt")
                    .SetMimeType("text/plain"));
    CreateEntry(AddEntriesMessage::TestEntryInfo(AddEntriesMessage::DIRECTORY,
                                                 std::string(), "A"));
    base::RunLoop().RunUntilIdle();
    return true;
  }

  bool PrepareDcimTestEntries(Profile* profile) {
    if (!CreateRootDirectory(profile))
      return false;

    CreateEntry(AddEntriesMessage::TestEntryInfo(AddEntriesMessage::DIRECTORY,
                                                 "", "DCIM"));
    CreateEntry(AddEntriesMessage::TestEntryInfo(AddEntriesMessage::FILE,
                                                 "image2.png", "image2.png")
                    .SetMimeType("image/png"));
    CreateEntry(AddEntriesMessage::TestEntryInfo(
                    AddEntriesMessage::FILE, "image3.jpg", "DCIM/image3.jpg")
                    .SetMimeType("image/png"));
    CreateEntry(AddEntriesMessage::TestEntryInfo(AddEntriesMessage::FILE,
                                                 "text.txt", "DCIM/hello.txt")
                    .SetMimeType("text/plain"));
    base::RunLoop().RunUntilIdle();
    return true;
  }

  bool Mount(Profile* profile) override {
    if (!MountSetup(profile))
      return false;

    // Expose the mount point with the given volume and device type.
    VolumeManager::Get(profile)->AddVolumeForTesting(root_path(), volume_type_,
                                                     device_type_, read_only_);
    base::RunLoop().RunUntilIdle();
    return true;
  }

  void Unmount(Profile* profile) {
    VolumeManager::Get(profile)->RemoveVolumeForTesting(
        root_path(), volume_type_, device_type_, read_only_);
  }

 protected:
  storage::ExternalMountPoints* GetMountPoints() {
    return storage::ExternalMountPoints::GetSystemInstance();
  }

  bool MountSetup(Profile* profile) {
    if (!CreateRootDirectory(profile))
      return false;

    // Revoke name() mount point first, then re-add its mount point.
    GetMountPoints()->RevokeFileSystem(name());
    const bool added = GetMountPoints()->RegisterFileSystem(
        name(), storage::kFileSystemTypeNativeLocal,
        storage::FileSystemMountOption(), root_path());
    if (!added)
      return false;

    return true;
  }

  const VolumeType volume_type_;
  const chromeos::DeviceType device_type_;
  const bool read_only_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeTestVolume);
};

// Removable TestVolume: local test volume for external media devices.
class RemovableTestVolume : public FakeTestVolume {
 public:
  RemovableTestVolume(const std::string& name,
                      VolumeType volume_type,
                      chromeos::DeviceType device_type,
                      const base::FilePath& device_path,
                      const std::string& drive_label,
                      const std::string& file_system_type)
      : FakeTestVolume(name, volume_type, device_type),
        device_path_(device_path),
        drive_label_(drive_label),
        file_system_type_(file_system_type) {}
  ~RemovableTestVolume() override = default;

  bool Mount(Profile* profile) override {
    if (!MountSetup(profile))
      return false;

    // Expose the mount point with the given volume and device type.
    VolumeManager::Get(profile)->AddVolumeForTesting(
        root_path(), volume_type_, device_type_, read_only_, device_path_,
        drive_label_, file_system_type_);
    base::RunLoop().RunUntilIdle();
    return true;
  }

  void Unmount(Profile* profile) {
    VolumeManager::Get(profile)->RemoveVolumeForTesting(
        root_path(), volume_type_, device_type_, read_only_, device_path_,
        drive_label_, file_system_type_);
  }

 private:
  const base::FilePath device_path_;
  const std::string drive_label_;
  const std::string file_system_type_;

  DISALLOW_COPY_AND_ASSIGN(RemovableTestVolume);
};

// DriveFsTestVolume: test volume for Google Drive using DriveFS.
class DriveFsTestVolume : public TestVolume {
 public:
  explicit DriveFsTestVolume(Profile* original_profile)
      : TestVolume("drive"), original_profile_(original_profile) {}
  ~DriveFsTestVolume() override = default;

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    if (!CreateRootDirectory(profile))
      return nullptr;

    EXPECT_FALSE(profile_);
    profile_ = profile;

    EXPECT_FALSE(integration_service_);
    integration_service_ = new drive::DriveIntegrationService(
        profile, std::string(), root_path().Append("v1"),
        CreateDriveFsBootstrapListener());

    return integration_service_;
  }

  bool Mount(Profile* profile) {
    if (profile != profile_)
      return false;

    if (!integration_service_)
      return false;

    integration_service_->SetEnabled(true);
    CreateDriveFsBootstrapListener();
    return true;
  }

  void Unmount() { integration_service_->SetEnabled(false); }

  void CreateEntry(const AddEntriesMessage::TestEntryInfo& entry) {
    const base::FilePath target_path = GetTargetPathForTestEntry(entry);

    entries_.insert(std::make_pair(target_path, entry));
    auto relative_path = GetRelativeDrivePathForTestEntry(entry);
    auto original_name = relative_path.BaseName();
    switch (entry.type) {
      case AddEntriesMessage::FILE: {
        original_name = base::FilePath(entry.target_path).BaseName();
        if (entry.source_file_name.empty()) {
          ASSERT_TRUE(base::WriteFile(target_path, ""));
          break;
        }
        const base::FilePath source_path =
            TestVolume::GetTestDataFilePath(entry.source_file_name);
        ASSERT_TRUE(base::CopyFile(source_path, target_path))
            << "Copy from " << source_path.value() << " to "
            << target_path.value() << " failed.";
        break;
      }
      case AddEntriesMessage::DIRECTORY:
        ASSERT_TRUE(base::CreateDirectory(target_path))
            << "Failed to create a directory: " << target_path.value();
        break;
      case AddEntriesMessage::LINK:
        ASSERT_TRUE(base::CreateSymbolicLink(
            base::FilePath(entry.source_file_name), target_path))
            << "Failed to create a symlink from " << entry.source_file_name
            << " to " << target_path.value();
        break;
      case AddEntriesMessage::TEAM_DRIVE:
        ASSERT_TRUE(base::CreateDirectory(target_path))
            << "Failed to create a team drive: " << target_path.value();
        break;
      case AddEntriesMessage::COMPUTER:
        DCHECK(entry.folder_feature.is_machine_root);
        ASSERT_TRUE(base::CreateDirectory(target_path))
            << "Failed to create a computer: " << target_path.value();
        break;
    }
    fake_drivefs_helper_->fake_drivefs().SetMetadata(
        relative_path, entry.mime_type, original_name.value(), entry.pinned,
        entry.shared_option == AddEntriesMessage::SharedOption::SHARED ||
            entry.shared_option ==
                AddEntriesMessage::SharedOption::SHARED_WITH_ME,
        {entry.capabilities.can_share, entry.capabilities.can_copy,
         entry.capabilities.can_delete, entry.capabilities.can_rename,
         entry.capabilities.can_add_children},
        {entry.folder_feature.is_machine_root,
         entry.folder_feature.is_arbitrary_sync_folder,
         entry.folder_feature.is_external_media},
        "");

    ASSERT_TRUE(UpdateModifiedTime(entry));
  }

 private:
  base::RepeatingCallback<std::unique_ptr<drivefs::DriveFsBootstrapListener>()>
  CreateDriveFsBootstrapListener() {
    CHECK(base::CreateDirectory(GetMyDrivePath()));
    CHECK(base::CreateDirectory(GetTeamDriveGrandRoot()));
    CHECK(base::CreateDirectory(GetComputerGrandRoot()));

    if (!fake_drivefs_helper_) {
      fake_drivefs_helper_ = std::make_unique<drive::FakeDriveFsHelper>(
          original_profile_, mount_path());
    }

    return fake_drivefs_helper_->CreateFakeDriveFsListenerFactory();
  }

  // Updates the ModifiedTime of the entry, and its parent directories if
  // needed. Returns true on success.
  bool UpdateModifiedTime(const AddEntriesMessage::TestEntryInfo& entry) {
    const auto path = GetTargetPathForTestEntry(entry);
    if (!base::TouchFile(path, entry.last_modified_time,
                         entry.last_modified_time)) {
      return false;
    }

    // Update the modified time of parent directories because they may be
    // also affected by the update of child items.
    if (path.DirName() != GetTeamDriveGrandRoot() &&
        path.DirName() != GetComputerGrandRoot() &&
        path.DirName() != GetMyDrivePath() &&
        path.DirName() != GetSharedWithMePath()) {
      const auto it = entries_.find(path.DirName());
      if (it == entries_.end())
        return false;
      return UpdateModifiedTime(it->second);
    }

    return true;
  }

  base::FilePath GetTargetPathForTestEntry(
      const AddEntriesMessage::TestEntryInfo& entry) {
    const base::FilePath target_path =
        GetTargetBasePathForTestEntry(entry).Append(entry.target_path);
    if (entry.name_text != entry.target_path)
      return target_path.DirName().Append(entry.name_text);
    return target_path;
  }

  base::FilePath GetTargetBasePathForTestEntry(
      const AddEntriesMessage::TestEntryInfo& entry) {
    if (entry.shared_option == AddEntriesMessage::SHARED_WITH_ME ||
        entry.shared_option == AddEntriesMessage::NESTED_SHARED_WITH_ME) {
      return GetSharedWithMePath();
    }
    if (!entry.team_drive_name.empty()) {
      return GetTeamDrivePath(entry.team_drive_name);
    }
    if (!entry.computer_name.empty()) {
      return GetComputerPath(entry.computer_name);
    }
    return GetMyDrivePath();
  }

  base::FilePath GetRelativeDrivePathForTestEntry(
      const AddEntriesMessage::TestEntryInfo& entry) {
    const base::FilePath target_path = GetTargetPathForTestEntry(entry);
    base::FilePath drive_path("/");
    CHECK(mount_path().AppendRelativePath(target_path, &drive_path));
    return drive_path;
  }

  base::FilePath mount_path() { return root_path().Append("v2"); }

  base::FilePath GetMyDrivePath() { return mount_path().Append("root"); }

  base::FilePath GetTeamDriveGrandRoot() {
    return mount_path().Append("team_drives");
  }

  base::FilePath GetComputerGrandRoot() {
    return mount_path().Append("Computers");
  }

  base::FilePath GetSharedWithMePath() {
    return mount_path().Append(".files-by-id/123");
  }

  base::FilePath GetTeamDrivePath(const std::string& team_drive_name) {
    return GetTeamDriveGrandRoot().Append(team_drive_name);
  }

  base::FilePath GetComputerPath(const std::string& computer_name) {
    return GetComputerGrandRoot().Append(computer_name);
  }

  // Profile associated with this volume: not owned.
  Profile* profile_ = nullptr;
  // Integration service used for testing: not owned.
  drive::DriveIntegrationService* integration_service_ = nullptr;

  Profile* const original_profile_;
  std::map<base::FilePath, const AddEntriesMessage::TestEntryInfo> entries_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;

  DISALLOW_COPY_AND_ASSIGN(DriveFsTestVolume);
};

// DocumentsProviderTestVolume: test volume for Android DocumentsProvider.
class DocumentsProviderTestVolume : public TestVolume {
 public:
  DocumentsProviderTestVolume(
      const std::string& name,
      arc::FakeFileSystemInstance* const file_system_instance,
      const std::string& authority,
      const std::string& root_document_id,
      bool read_only)
      : TestVolume(name),
        file_system_instance_(file_system_instance),
        authority_(authority),
        root_document_id_(root_document_id),
        read_only_(read_only) {}
  DocumentsProviderTestVolume(
      arc::FakeFileSystemInstance* const file_system_instance,
      const std::string& authority,
      const std::string& root_document_id,
      bool read_only)
      : DocumentsProviderTestVolume("DocumentsProvider",
                                    file_system_instance,
                                    authority,
                                    root_document_id,
                                    read_only) {}
  ~DocumentsProviderTestVolume() override = default;

  virtual void CreateEntry(const AddEntriesMessage::TestEntryInfo& entry) {
    // Register a document to the fake FileSystemInstance.
    arc::FakeFileSystemInstance::Document document(
        authority_, entry.name_text, root_document_id_, entry.name_text,
        GetMimeType(entry), GetFileSize(entry),
        entry.last_modified_time.ToJavaTime(), entry.capabilities.can_delete,
        entry.capabilities.can_rename, entry.capabilities.can_add_children);
    file_system_instance_->AddDocument(document);
  }

  virtual bool Mount(Profile* profile) {
    RegisterRoot();

    // Tell VolumeManager that a new DocumentsProvider volume is added.
    VolumeManager::Get(profile)->OnDocumentsProviderRootAdded(
        authority_, root_document_id_, root_document_id_, name(), "", GURL(),
        read_only_, std::vector<std::string>());
    return true;
  }

 protected:
  arc::FakeFileSystemInstance* const file_system_instance_;
  const std::string authority_;
  const std::string root_document_id_;
  const bool read_only_;

  // Register a root document of this volume.
  void RegisterRoot() {
    arc::FakeFileSystemInstance::Document document(
        authority_, root_document_id_, "", "", arc::kAndroidDirectoryMimeType,
        0, 0);
    file_system_instance_->AddDocument(document);
  }

 private:
  int64_t GetFileSize(const AddEntriesMessage::TestEntryInfo& entry) {
    if (entry.type != AddEntriesMessage::FILE)
      return 0;

    int64_t file_size = 0;
    const base::FilePath source_path =
        TestVolume::GetTestDataFilePath(entry.source_file_name);
    bool success = base::GetFileSize(source_path, &file_size);
    return success ? file_size : 0;
  }

  std::string GetMimeType(const AddEntriesMessage::TestEntryInfo& entry) {
    return entry.type == AddEntriesMessage::FILE
               ? entry.mime_type
               : arc::kAndroidDirectoryMimeType;
  }

  DISALLOW_COPY_AND_ASSIGN(DocumentsProviderTestVolume);
};

// MediaViewTestVolume: Test volume for the "media views": Audio, Images and
// Videos.
class MediaViewTestVolume : public DocumentsProviderTestVolume {
 public:
  MediaViewTestVolume(arc::FakeFileSystemInstance* const file_system_instance,
                      const std::string& authority,
                      const std::string& root_document_id)
      : DocumentsProviderTestVolume(root_document_id,
                                    file_system_instance,
                                    authority,
                                    root_document_id,
                                    true /* read_only */) {}

  ~MediaViewTestVolume() override = default;

  bool Mount(Profile* profile) override {
    RegisterRoot();
    return VolumeManager::Get(profile)->RegisterMediaViewForTesting(
        root_document_id_);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaViewTestVolume);
};

class MockSmbFsMounter : public smbfs::SmbFsMounter {
 public:
  MOCK_METHOD(void,
              Mount,
              (smbfs::SmbFsMounter::DoneCallback callback),
              (override));
};

class MockSmbFsImpl : public smbfs::mojom::SmbFs {
 public:
  explicit MockSmbFsImpl(mojo::PendingReceiver<smbfs::mojom::SmbFs> pending)
      : receiver_(this, std::move(pending)) {}

  MOCK_METHOD(void,
              RemoveSavedCredentials,
              (RemoveSavedCredentialsCallback),
              (override));

  MOCK_METHOD(void,
              DeleteRecursively,
              (const base::FilePath&, DeleteRecursivelyCallback),
              (override));

 private:
  mojo::Receiver<smbfs::mojom::SmbFs> receiver_;
};

// SmbfsTestVolume: Test volume for FUSE-based SMB file shares.
class SmbfsTestVolume : public LocalTestVolume {
 public:
  SmbfsTestVolume() : LocalTestVolume("smbfs") {}
  ~SmbfsTestVolume() override = default;

  // Create root dir so entries can be created, but volume is not mounted.
  bool Initialize(Profile* profile) { return CreateRootDirectory(profile); }

  bool Mount(Profile* profile) override {
    // Only support mounting this volume once.
    CHECK(!mock_smbfs_);
    if (!CreateRootDirectory(profile)) {
      return false;
    }

    chromeos::smb_client::SmbService* smb_service =
        chromeos::smb_client::SmbServiceFactory::Get(profile);
    {
      base::RunLoop run_loop;
      smb_service->OnSetupCompleteForTesting(run_loop.QuitClosure());
      run_loop.Run();
    }
    {
      // Share gathering needs to complete at least once before a share can be
      // mounted.
      base::RunLoop run_loop;
      smb_service->GatherSharesInNetwork(
          base::DoNothing(),
          base::BindLambdaForTesting(
              [&run_loop](const std::vector<chromeos::smb_client::SmbUrl>&
                              shares_gathered,
                          bool done) {
                if (done) {
                  run_loop.Quit();
                }
              }));
      run_loop.Run();
    }

    // Inject a mounter creation callback so that smbfs startup can be faked
    // out.
    smb_service->SetSmbFsMounterCreationCallbackForTesting(base::BindRepeating(
        &SmbfsTestVolume::CreateMounter, base::Unretained(this)));

    bool success = false;
    chromeos::file_system_provider::MountOptions mount_options;
    mount_options.display_name = "SMB Share";
    base::RunLoop run_loop;
    smb_service->Mount(
        mount_options, base::FilePath("smb://server/share"), "" /* username */,
        "" /* password */, false /* use_chromad_kerberos */,
        false /* should_open_file_manager_after_mount */,
        false /* save_credentials */,
        base::BindLambdaForTesting(
            [&](chromeos::smb_client::SmbMountResult result) {
              success =
                  (result == chromeos::smb_client::SmbMountResult::kSuccess);
              run_loop.Quit();
            }));
    run_loop.Run();
    return success;
  }

  const base::FilePath& mount_path() const { return root_path(); }

 private:
  std::unique_ptr<smbfs::SmbFsMounter> CreateMounter(
      const std::string& share_path,
      const std::string& mount_dir_name,
      const chromeos::smb_client::SmbFsShare::MountOptions& options,
      smbfs::SmbFsHost::Delegate* delegate) {
    std::unique_ptr<MockSmbFsMounter> mock_mounter =
        std::make_unique<MockSmbFsMounter>();
    EXPECT_CALL(*mock_mounter, Mount(_))
        .WillOnce([this,
                   delegate](smbfs::SmbFsMounter::DoneCallback mount_callback) {
          mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
          mock_smbfs_ = std::make_unique<MockSmbFsImpl>(
              smbfs_remote.BindNewPipeAndPassReceiver());

          std::move(mount_callback)
              .Run(smbfs::mojom::MountError::kOk,
                   std::make_unique<smbfs::SmbFsHost>(
                       std::make_unique<chromeos::disks::MountPoint>(
                           mount_path(),
                           chromeos::disks::DiskMountManager::GetInstance()),
                       delegate, std::move(smbfs_remote),
                       delegate_.BindNewPipeAndPassReceiver()));
        });
    return std::move(mock_mounter);
  }

  std::unique_ptr<MockSmbFsImpl> mock_smbfs_;
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(SmbfsTestVolume);
};

FileManagerBrowserTestBase::FileManagerBrowserTestBase() = default;

FileManagerBrowserTestBase::~FileManagerBrowserTestBase() = default;

void FileManagerBrowserTestBase::SetUp() {
  net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
  extensions::ExtensionApiTest::SetUp();
}

void FileManagerBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  const Options options = GetOptions();

  // Use a fake audio stream crbug.com/835626
  command_line->AppendSwitch(switches::kDisableAudioOutput);

  if (!options.browser) {
    // Don't sink time into showing an unused browser window.
    // InProcessBrowserTest::browser() will be null.
    command_line->AppendSwitch(switches::kNoStartupWindow);

    // Without a browser window, opening an app window, then closing it will
    // trigger browser shutdown. Usually this is fine, except it also prevents
    // any _new_ app window being created, should a test want to do that.
    // (At the time of writing, exactly one does).
    // Although in this path no browser is created (and so one can never
    // close..), setting this to false prevents InProcessBrowserTest from adding
    // the kDisableZeroBrowsersOpenForTests flag, which would prevent
    // chrome_browser_main_chromeos from adding the keepalive that normally
    // stops chromeos from shutting down unexpectedly.
    set_exit_when_last_browser_closes(false);
  }

  if (options.guest_mode == IN_GUEST_MODE) {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitchNative(chromeos::switches::kLoginUser, "$guest");
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kIncognito);
    set_chromeos_user_ = false;
  }

  if (options.guest_mode == IN_INCOGNITO) {
    command_line->AppendSwitch(switches::kIncognito);
  }

  if (options.offline) {
    command_line->AppendSwitchASCII(chromeos::switches::kShillStub, "clear=1");
  }

  // TODO(crbug.com/937746): See crbug.com/1081581 for context, but
  // the FilesApp does not work when custom elements v0 are enabled.
  // Make sure they are disabled here. Remove this once WCv0 features
  // are removed completely.
  command_line->AppendSwitchASCII(switches::kDisableBlinkFeatures,
                                  "ShadowDOMV0,CustomElementsV0,HTMLImports");

  std::vector<base::Feature> enabled_features;
  std::vector<base::Feature> disabled_features;

  if (options.files_ng) {
    enabled_features.push_back(chromeos::features::kFilesNG);
  } else {
    disabled_features.push_back(chromeos::features::kFilesNG);
  }

  if (options.arc) {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  // Make sure to run the ARC storage UI toast tests.
  enabled_features.push_back(arc::kUsbStorageUIFeature);

  if (options.documents_provider) {
    enabled_features.push_back(arc::kEnableDocumentsProviderInFilesAppFeature);
  } else {
    disabled_features.push_back(arc::kEnableDocumentsProviderInFilesAppFeature);
  }

  if (options.unified_media_view) {
    enabled_features.push_back(chromeos::features::kUnifiedMediaView);
  } else {
    disabled_features.push_back(chromeos::features::kUnifiedMediaView);
  }

  if (options.smbfs) {
    enabled_features.push_back(features::kSmbFs);
  }

  if (options.zip_no_nacl) {
    enabled_features.push_back(chromeos::features::kFilesZipMount);
    enabled_features.push_back(chromeos::features::kFilesZipPack);
    enabled_features.push_back(chromeos::features::kFilesZipUnpack);
  } else {
    disabled_features.push_back(chromeos::features::kFilesZipMount);
    disabled_features.push_back(chromeos::features::kFilesZipPack);
    disabled_features.push_back(chromeos::features::kFilesZipUnpack);
  }

  if (options.enable_sharesheet) {
    enabled_features.push_back(features::kSharesheet);
  }

  // This is destroyed in |TearDown()|. We cannot initialize this in the
  // constructor due to this feature values' above dependence on virtual
  // method calls, but by convention subclasses of this fixture may initialize
  // ScopedFeatureList instances in their own constructor. Ensuring construction
  // here and destruction in |TearDown()| ensures that we preserve an acceptable
  // relative lifetime ordering between this ScopedFeatureList and those of any
  // subclasses.
  feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  feature_list_->InitWithFeatures(enabled_features, disabled_features);

  extensions::ExtensionApiTest::SetUpCommandLine(command_line);
}

bool FileManagerBrowserTestBase::SetUpUserDataDirectory() {
  if (GetOptions().guest_mode == IN_GUEST_MODE)
    return true;

  return drive::SetUpUserDataDirectoryForDriveFsTest();
}

void FileManagerBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();

  local_volume_ = std::make_unique<DownloadsTestVolume>();

  if (GetOptions().guest_mode == IN_GUEST_MODE)
    return;

  create_drive_integration_service_ =
      base::Bind(&FileManagerBrowserTestBase::CreateDriveIntegrationService,
                 base::Unretained(this));
  service_factory_for_test_ = std::make_unique<
      drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
      &create_drive_integration_service_);
}

void FileManagerBrowserTestBase::SetUpOnMainThread() {
  const Options options = GetOptions();

  // Must happen after the browser process is created because instantiating
  // the factory will instantiate ExtensionSystemFactory which depends on
  // ExtensionsBrowserClient setup in BrowserProcessImpl.
  sync_file_system::SyncFileSystemServiceFactory::GetInstance()
      ->set_mock_remote_file_service(
          std::make_unique<::testing::NiceMock<
              sync_file_system::MockRemoteFileSyncService>>());

  extensions::ExtensionApiTest::SetUpOnMainThread();
  CHECK(profile());
  CHECK_EQ(!!browser(), options.browser);

  if (!options.mount_volumes) {
    VolumeManager::Get(profile())->RemoveDownloadsDirectoryForTesting();
  } else {
    CHECK(local_volume_->Mount(profile()));
  }

  if (options.guest_mode != IN_GUEST_MODE) {
    // Start the embedded test server to serve the mocked CWS widget container.
    CHECK(embedded_test_server()->Start());
    drive_volume_ = drive_volumes_[profile()->GetOriginalProfile()].get();
    if (options.mount_volumes) {
      test_util::WaitUntilDriveMountPointIsAdded(profile());
    }

    // Init crostini.  Set VM and container running for testing, and register
    // CustomMountPointCallback.
    crostini_volume_ = std::make_unique<CrostiniTestVolume>();
    if (options.guest_mode != IN_INCOGNITO) {
      crostini_features_.set_ui_allowed(true);
      crostini_features_.set_enabled(true);
      crostini_features_.set_root_access_allowed(true);
      crostini_features_.set_export_import_ui_allowed(true);
    }
    crostini::CrostiniManager* crostini_manager =
        crostini::CrostiniManager::GetForProfile(
            profile()->GetOriginalProfile());
    crostini_manager->set_skip_restart_for_testing();
    crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
    crostini_manager->AddRunningContainerForTesting(
        crostini::kCrostiniDefaultVmName,
        crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                                "testuser", "/home/testuser",
                                "PLACEHOLDER_IP"));
    chromeos::DBusThreadManager* dbus_thread_manager =
        chromeos::DBusThreadManager::Get();
    static_cast<chromeos::FakeCrosDisksClient*>(
        dbus_thread_manager->GetCrosDisksClient())
        ->AddCustomMountPointCallback(
            base::BindRepeating(&FileManagerBrowserTestBase::MaybeMountCrostini,
                                base::Unretained(this)));

    if (arc::IsArcAvailable()) {
      // When ARC is marked as available, we create fake FileSystemInstance and
      // register it so that ARC-related services can work without real ARC
      // container.
      arc_file_system_instance_ =
          std::make_unique<arc::FakeFileSystemInstance>();
      arc::ArcServiceManager::Get()
          ->arc_bridge_service()
          ->file_system()
          ->SetInstance(arc_file_system_instance_.get());
      arc::WaitForInstanceReady(
          arc::ArcServiceManager::Get()->arc_bridge_service()->file_system());
      ASSERT_TRUE(arc_file_system_instance_->InitCalled());

      if (options.documents_provider) {
        // Though we can have multiple DocumentsProvider volumes, only one
        // volume is created and mounted for now.
        documents_provider_volume_ =
            std::make_unique<DocumentsProviderTestVolume>(
                arc_file_system_instance_.get(), "com.example.documents",
                "root", false /* read_only */);
        if (options.mount_volumes) {
          documents_provider_volume_->Mount(profile());
        }
      }
    } else {
      // When ARC is not available, "Android Files" will not be mounted.
      // We need to mount testing volume here.
      android_files_volume_ = std::make_unique<AndroidFilesTestVolume>();
      if (options.mount_volumes) {
        android_files_volume_->Mount(profile());
      }
    }

    if (options.guest_mode != IN_INCOGNITO) {
      if (options.observe_file_tasks) {
        file_tasks_observer_ =
            std::make_unique<testing::StrictMock<MockFileTasksObserver>>(
                profile());
      }
    } else {
      EXPECT_FALSE(file_tasks::FileTasksNotifier::GetForProfile(profile()));
    }
  }

  if (options.smbfs) {
    smbfs_volume_ = std::make_unique<SmbfsTestVolume>();
  }

  display_service_ =
      std::make_unique<NotificationDisplayServiceTester>(profile());

  content::NetworkConnectionChangeSimulator network_change_simulator;
  network_change_simulator.SetConnectionType(
      options.offline ? network::mojom::ConnectionType::CONNECTION_NONE
                      : network::mojom::ConnectionType::CONNECTION_ETHERNET);

  // The test resources are setup: enable and add default ChromeOS component
  // extensions now and not before: crbug.com/831074, crbug.com/804413
  test::AddDefaultComponentExtensionsOnMainThread(profile());

  // For tablet mode tests, enable the Ash virtual keyboard.
  if (options.tablet_mode) {
    EnableVirtualKeyboard();
  }

  select_factory_ = new SelectFileDialogExtensionTestFactory();
  ui::SelectFileDialog::SetFactory(select_factory_);
}

void FileManagerBrowserTestBase::TearDownOnMainThread() {
  file_tasks_observer_.reset();
  select_factory_ = nullptr;
  ui::SelectFileDialog::SetFactory(nullptr);
}

void FileManagerBrowserTestBase::TearDown() {
  extensions::ExtensionApiTest::TearDown();
  feature_list_.reset();
}

void FileManagerBrowserTestBase::StartTest() {
  LOG(INFO) << "FileManagerBrowserTest::StartTest " << GetFullTestCaseName();
  static const base::FilePath test_extension_dir =
      base::FilePath(FILE_PATH_LITERAL("ui/file_manager/integration_tests"));
  LaunchExtension(test_extension_dir, GetTestExtensionManifestName());
  RunTestMessageLoop();
}

void FileManagerBrowserTestBase::LaunchExtension(const base::FilePath& path,
                                                 const char* manifest_name) {
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));

  const base::FilePath source_path = source_dir.Append(path);
  const extensions::Extension* const extension_launched =
      LoadExtensionAsComponentWithManifest(source_path, manifest_name);
  CHECK(extension_launched) << "Launching: " << manifest_name;
}

void FileManagerBrowserTestBase::RunTestMessageLoop() {
  FileManagerTestMessageListener listener;

  while (true) {
    auto message = listener.GetNextMessage();

    if (message.type == extensions::NOTIFICATION_EXTENSION_TEST_PASSED)
      return;  // Test PASSED.
    if (message.type == extensions::NOTIFICATION_EXTENSION_TEST_FAILED) {
      ADD_FAILURE() << message.message;
      return;  // Test FAILED.
    }

    // If the message in JSON format has no command, ignore it
    // but note a reply is required: use std::string().
    const auto json = base::JSONReader::ReadDeprecated(message.message);
    const base::DictionaryValue* dictionary = nullptr;
    std::string command;
    if (!json || !json->GetAsDictionary(&dictionary) ||
        !dictionary->GetString("name", &command)) {
      message.function->Reply(std::string());
      continue;
    }

    // Process the command, reply with the result.
    std::string result;
    OnCommand(command, *dictionary, &result);
    if (!HasFatalFailure()) {
      message.function->Reply(result);
      continue;
    }

    // Test FAILED: while processing the command.
    LOG(INFO) << "[FAILED] " << GetTestCaseName();
    return;
  }
}

// NO_THREAD_SAFETY_ANALYSIS: Locking depends on runtime commands, the static
// checker cannot assess it.
void FileManagerBrowserTestBase::OnCommand(const std::string& name,
                                           const base::DictionaryValue& value,
                                           std::string* output)
    NO_THREAD_SAFETY_ANALYSIS {
  const Options options = GetOptions();

  base::ScopedAllowBlockingForTesting allow_blocking;

  if (name == "isInGuestMode") {
    // Obtain if the test runs in guest or incognito mode.
    LOG(INFO) << GetTestCaseName() << " is in " << options.guest_mode
              << " mode";
    *output = options.guest_mode == NOT_IN_GUEST_MODE ? "false" : "true";

    return;
  }

  if (name == "launchAppOnLocalFolder") {
    GetLocalPathMessage message;
    ASSERT_TRUE(GetLocalPathMessage::ConvertJSONValue(value, &message));

    base::FilePath folder_path =
        file_manager::util::GetMyFilesFolderForProfile(profile());
    folder_path = folder_path.AppendASCII(message.local_path);

    platform_util::OpenItem(profile(), folder_path, platform_util::OPEN_FOLDER,
                            platform_util::OpenOperationCallback());

    return;
  }

  if (name == "launchAppOnDrive") {
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    ASSERT_TRUE(integration_service && integration_service->is_enabled());
    base::FilePath mount_path =
        integration_service->GetMountPointPath().AppendASCII("root");

    platform_util::OpenItem(profile(), mount_path, platform_util::OPEN_FOLDER,
                            platform_util::OpenOperationCallback());

    return;
  }

  if (name == "zipArchiverLoaded") {
    if (options.zip) {
      LOG(INFO) << "Preloading zip archiver NaCl module";
      auto event = std::make_unique<extensions::Event>(
          extensions::events::FOR_TEST,
          extensions::api::test::OnMessage::kEventName,
          base::ListValue::From(base::JSONReader::ReadDeprecated(
              R"([{"data": "preloadZip", "lastMessage": false}])")),
          profile());
      extensions::EventRouter::Get(profile())->DispatchEventToExtension(
          kZipArchiverId, std::move(event));
    }
    return;
  }

  if (name == "getRootPaths") {
    // Obtain the root paths.
    const auto downloads_root =
        util::GetDownloadsMountPointName(profile()) + "/Downloads";

    base::DictionaryValue dictionary;
    dictionary.SetString("downloads", "/" + downloads_root);

    if (!profile()->IsGuestSession()) {
      auto* drive_integration_service =
          drive::DriveIntegrationServiceFactory::GetForProfile(profile());
      if (drive_integration_service->IsMounted()) {
        const auto drive_mount_name =
            base::FilePath(drive_integration_service->GetMountPointPath())
                .BaseName();
        dictionary.SetString(
            "drive", base::StrCat({"/", drive_mount_name.value(), "/root"}));
      }
      if (android_files_volume_) {
        dictionary.SetString("android_files",
                             "/" + util::GetAndroidFilesMountPointName());
      }
    }
    base::JSONWriter::Write(dictionary, output);
    return;
  }

  if (name == "getTestName") {
    // Obtain the test case name.
    *output = GetTestCaseName();
    return;
  }

  if (name == "getCwsWidgetContainerMockUrl") {
    // Obtain the mock CWS widget container URL and URL.origin.
    const GURL url = embedded_test_server()->GetURL(
        "/chromeos/file_manager/cws_container_mock/index.html");
    std::string origin = url.GetOrigin().spec();
    if (*origin.rbegin() == '/')  // Strip origin trailing '/'.
      origin.resize(origin.length() - 1);

    base::DictionaryValue dictionary;
    dictionary.SetString("url", url.spec());
    dictionary.SetString("origin", origin);

    base::JSONWriter::Write(dictionary, output);
    return;
  }

  if (name == "addEntries") {
    // Add the message.entries to the message.volume.
    AddEntriesMessage message;
    ASSERT_TRUE(AddEntriesMessage::ConvertJSONValue(value, &message));

    for (size_t i = 0; i < message.entries.size(); ++i) {
      switch (message.volume) {
        case AddEntriesMessage::LOCAL_VOLUME:
          local_volume_->CreateEntry(*message.entries[i]);
          break;
        case AddEntriesMessage::CROSTINI_VOLUME:
          CHECK(crostini_volume_);
          ASSERT_TRUE(crostini_volume_->Initialize(profile()));
          crostini_volume_->CreateEntry(*message.entries[i]);
          break;
        case AddEntriesMessage::DRIVE_VOLUME:
          if (drive_volume_) {
            drive_volume_->CreateEntry(*message.entries[i]);
          } else {
            CHECK_EQ(options.guest_mode, IN_GUEST_MODE)
                << "Add entry, but no Drive volume";
          }
          break;
        case AddEntriesMessage::USB_VOLUME:
          if (usb_volume_) {
            usb_volume_->CreateEntry(*message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no USB volume.";
          }
          break;
        case AddEntriesMessage::ANDROID_FILES_VOLUME:
          if (android_files_volume_) {
            android_files_volume_->CreateEntry(*message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no Android files volume.";
          }
          break;
        case AddEntriesMessage::DOCUMENTS_PROVIDER_VOLUME:
          if (documents_provider_volume_) {
            documents_provider_volume_->CreateEntry(*message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no DocumentsProvider volume.";
          }
          break;
        case AddEntriesMessage::MEDIA_VIEW_AUDIO:
          if (media_view_audio_) {
            media_view_audio_->CreateEntry(*message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no MediaView Audio volume.";
          }
          break;
        case AddEntriesMessage::MEDIA_VIEW_IMAGES:
          if (media_view_images_) {
            media_view_images_->CreateEntry(*message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no MediaView Images volume.";
          }
          break;
        case AddEntriesMessage::MEDIA_VIEW_VIDEOS:
          if (media_view_videos_) {
            media_view_videos_->CreateEntry(*message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no MediaView Videos volume.";
          }
          break;
        case AddEntriesMessage::SMBFS_VOLUME:
          CHECK(smbfs_volume_);
          ASSERT_TRUE(smbfs_volume_->Initialize(profile()));
          smbfs_volume_->CreateEntry(*message.entries[i]);
          break;
      }
    }

    return;
  }

  if (name == "mountFakeUsb" || name == "mountFakeUsbEmpty" ||
      name == "mountFakeUsbDcim") {
    std::string file_system = "ext4";
    const std::string* file_system_param = value.FindStringKey("filesystem");
    if (file_system_param) {
      file_system = *file_system_param;
    }
    usb_volume_ = std::make_unique<RemovableTestVolume>(
        "fake-usb", VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
        chromeos::DEVICE_TYPE_USB, base::FilePath(), "FAKEUSB", file_system);

    if (name == "mountFakeUsb")
      ASSERT_TRUE(usb_volume_->PrepareTestEntries(profile()));
    else if (name == "mountFakeUsbDcim")
      ASSERT_TRUE(usb_volume_->PrepareDcimTestEntries(profile()));

    ASSERT_TRUE(usb_volume_->Mount(profile()));
    return;
  }

  if (name == "unmountUsb") {
    DCHECK(usb_volume_);
    usb_volume_->Unmount(profile());
    return;
  }

  if (name == "mountUsbWithPartitions") {
    // Create a device path to mimic a realistic device path.
    constexpr char kDevicePath[] =
        "sys/devices/pci0000:00/0000:00:14.0/usb1/1-2/1-2.2/1-2.2:1.0/host0/"
        "target0:0:0/0:0:0:0";
    const base::FilePath device_path(kDevicePath);

    // Create partition volumes with the same device path and drive label.
    partition_1_ = std::make_unique<RemovableTestVolume>(
        "partition-1", VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
        chromeos::DEVICE_TYPE_USB, device_path, "Drive Label", "ext4");
    partition_2_ = std::make_unique<RemovableTestVolume>(
        "partition-2", VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
        chromeos::DEVICE_TYPE_USB, device_path, "Drive Label", "ext4");

    // Create fake entries on partitions.
    ASSERT_TRUE(partition_1_->PrepareTestEntries(profile()));
    ASSERT_TRUE(partition_2_->PrepareTestEntries(profile()));

    ASSERT_TRUE(partition_1_->Mount(profile()));
    ASSERT_TRUE(partition_2_->Mount(profile()));
    return;
  }

  if (name == "mountUsbWithMultiplePartitionTypes") {
    // Create a device path to mimic a realistic device path.
    constexpr char kDevicePath[] =
        "sys/devices/pci0000:00/0000:00:14.0/usb1/1-2/1-2.2/1-2.2:1.0/host0/"
        "target0:0:0/0:0:0:0";
    const base::FilePath device_path(kDevicePath);

    // Create partition volumes with the same device path.
    partition_1_ = std::make_unique<RemovableTestVolume>(
        "partition-1", VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
        chromeos::DEVICE_TYPE_USB, device_path, "Drive Label", "ntfs");
    partition_2_ = std::make_unique<RemovableTestVolume>(
        "partition-2", VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
        chromeos::DEVICE_TYPE_USB, device_path, "Drive Label", "ext4");
    partition_3_ = std::make_unique<RemovableTestVolume>(
        "partition-3", VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
        chromeos::DEVICE_TYPE_USB, device_path, "Drive Label", "vfat");

    // Create fake entries on partitions.
    ASSERT_TRUE(partition_1_->PrepareTestEntries(profile()));
    ASSERT_TRUE(partition_2_->PrepareTestEntries(profile()));
    ASSERT_TRUE(partition_3_->PrepareTestEntries(profile()));

    ASSERT_TRUE(partition_1_->Mount(profile()));
    ASSERT_TRUE(partition_2_->Mount(profile()));
    ASSERT_TRUE(partition_3_->Mount(profile()));
    return;
  }

  if (name == "unmountPartitions") {
    DCHECK(partition_1_);
    DCHECK(partition_2_);
    partition_1_->Unmount(profile());
    partition_2_->Unmount(profile());
    return;
  }

  if (name == "mountFakeMtp" || name == "mountFakeMtpEmpty") {
    mtp_volume_ = std::make_unique<FakeTestVolume>(
        "fake-mtp", VOLUME_TYPE_MTP, chromeos::DEVICE_TYPE_UNKNOWN);

    if (name == "mountFakeMtp")
      ASSERT_TRUE(mtp_volume_->PrepareTestEntries(profile()));

    ASSERT_TRUE(mtp_volume_->Mount(profile()));
    return;
  }

  if (name == "mountDrive") {
    ASSERT_TRUE(drive_volume_->Mount(profile()));
    return;
  }

  if (name == "unmountDrive") {
    drive_volume_->Unmount();
    return;
  }

  if (name == "mountDownloads") {
    ASSERT_TRUE(local_volume_->Mount(profile()));
    return;
  }

  if (name == "unmountDownloads") {
    local_volume_->Unmount(profile());
    return;
  }

  if (name == "mountMediaView") {
    CHECK(arc::IsArcAvailable())
        << "ARC required for mounting media view volumes";

    media_view_images_ = std::make_unique<MediaViewTestVolume>(
        arc_file_system_instance_.get(),
        "com.android.providers.media.documents", arc::kImagesRootDocumentId);
    media_view_videos_ = std::make_unique<MediaViewTestVolume>(
        arc_file_system_instance_.get(),
        "com.android.providers.media.documents", arc::kVideosRootDocumentId);
    media_view_audio_ = std::make_unique<MediaViewTestVolume>(
        arc_file_system_instance_.get(),
        "com.android.providers.media.documents", arc::kAudioRootDocumentId);

    ASSERT_TRUE(media_view_images_->Mount(profile()));
    ASSERT_TRUE(media_view_videos_->Mount(profile()));
    ASSERT_TRUE(media_view_audio_->Mount(profile()));
    return;
  }

  if (name == "mountPlayFiles") {
    DCHECK(android_files_volume_);
    android_files_volume_->Mount(profile());
    return;
  }

  if (name == "unmountPlayFiles") {
    DCHECK(android_files_volume_);
    android_files_volume_->Unmount(profile());
    return;
  }

  if (name == "mountSmbfs") {
    CHECK(smbfs_volume_);
    ASSERT_TRUE(smbfs_volume_->Mount(profile()));
    return;
  }

  if (name == "setDriveEnabled") {
    bool enabled;
    ASSERT_TRUE(value.GetBoolean("enabled", &enabled));
    profile()->GetPrefs()->SetBoolean(drive::prefs::kDisableDrive, !enabled);
    return;
  }

  if (name == "setPdfPreviewEnabled") {
    bool enabled;
    ASSERT_TRUE(value.GetBoolean("enabled", &enabled));
    profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysOpenPdfExternally,
                                      !enabled);
    return;
  }

  if (name == "setCrostiniEnabled") {
    bool enabled;
    ASSERT_TRUE(value.GetBoolean("enabled", &enabled));
    profile()->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled,
                                      enabled);
    return;
  }

  if (name == "setCrostiniRootAccessAllowed") {
    bool enabled;
    ASSERT_TRUE(value.GetBoolean("enabled", &enabled));
    crostini_features_.set_root_access_allowed(enabled);
    return;
  }

  if (name == "setCrostiniExportImportAllowed") {
    bool enabled;
    ASSERT_TRUE(value.GetBoolean("enabled", &enabled));
    crostini_features_.set_export_import_ui_allowed(enabled);
    return;
  }

  if (name == "useCellularNetwork") {
    net::NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChangeForTests(
        net::NetworkChangeNotifier::GetMaxBandwidthMbpsForConnectionSubtype(
            net::NetworkChangeNotifier::SUBTYPE_HSPA),
        net::NetworkChangeNotifier::CONNECTION_3G);
    return;
  }

  if (name == "clickNotificationButton") {
    std::string extension_id;
    std::string notification_id;
    ASSERT_TRUE(value.GetString("extensionId", &extension_id));
    ASSERT_TRUE(value.GetString("notificationId", &notification_id));

    const std::string delegate_id = extension_id + "-" + notification_id;
    base::Optional<message_center::Notification> notification =
        display_service_->GetNotification(delegate_id);
    EXPECT_TRUE(notification);

    int index;
    ASSERT_TRUE(value.GetInteger("index", &index));
    display_service_->SimulateClick(NotificationHandler::Type::EXTENSION,
                                    delegate_id, index, base::nullopt);
    return;
  }

  if (name == "launchProviderExtension") {
    std::string manifest;
    ASSERT_TRUE(value.GetString("manifest", &manifest));
    LaunchExtension(base::FilePath(FILE_PATH_LITERAL(
                        "ui/file_manager/integration_tests/testing_provider")),
                    manifest.c_str());
    return;
  }

  if (name == "dispatchNativeMediaKey") {
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_MEDIA_PLAY_PAUSE, 0);

    // Try to dispatch the event close-to-native without pulling in too many
    // dependencies (i.e. X11/Ozone/Wayland/Mus). aura::WindowTreeHost is pretty
    // high up in the dispatch stack, but we might need event_injector.mojom
    // for a more realistic Mus dispatch.
    const auto& app_windows =
        extensions::AppWindowRegistry::Get(profile())->app_windows();
    ASSERT_FALSE(app_windows.empty());
    app_windows.front()->GetNativeWindow()->GetHost()->DispatchKeyEventPostIME(
        &key_event);
    *output = "mediaKeyDispatched";
    return;
  }

  if (name == "dispatchTabKey") {
    // Read optional modifier parameter |shift|.
    bool shift;
    if (!value.GetBoolean("shift", &shift)) {
      shift = false;
    }

    int flag = shift ? ui::EF_SHIFT_DOWN : 0;
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_TAB, flag);
    // Try to dispatch the event close-to-native without pulling in too many
    // dependencies (i.e. X11/Ozone/Wayland/Mus). aura::WindowTreeHost is pretty
    // high up in the dispatch stack, but we might need event_injector.mojom
    // for a more realistic Mus dispatch.
    const auto& app_windows =
        extensions::AppWindowRegistry::Get(profile())->app_windows();
    aura::WindowTreeHost* host = nullptr;
    if (app_windows.empty()) {
      ASSERT_TRUE(select_factory_);
      views::Widget* widget = select_factory_->GetLastWidget();
      ASSERT_TRUE(widget);
      host = widget->GetNativeWindow()->GetHost();
    } else {
      host = app_windows.front()->GetNativeWindow()->GetHost();
    }
    ASSERT_TRUE(host);
    host->DispatchKeyEventPostIME(&key_event);
    *output = "tabKeyDispatched";
    return;
  }

  if (name == "simulateClick") {
    int click_x;
    int click_y;
    ASSERT_TRUE(value.GetInteger("clickX", &click_x));
    ASSERT_TRUE(value.GetInteger("clickY", &click_y));

    const auto& app_windows =
        extensions::AppWindowRegistry::Get(profile())->app_windows();
    auto* last_window = app_windows.front();

    SimulateMouseClickAt(last_window->web_contents(), 0 /* modifiers */,
                         blink::WebMouseEvent::Button::kLeft,
                         gfx::Point(click_x, click_y));
    return;
  }

  if (name == "getAppWindowId") {
    std::string window_url;
    ASSERT_TRUE(value.GetString("windowUrl", &window_url));

    const auto& app_windows =
        extensions::AppWindowRegistry::Get(profile())->app_windows();
    ASSERT_FALSE(app_windows.empty());
    *output = "none";
    for (auto* window : app_windows) {
      if (!window->web_contents())
        continue;

      if (window->web_contents()->GetLastCommittedURL() == window_url) {
        *output = base::NumberToString(window->session_id().id());
        break;
      }
    }
    return;
  }

  if (name == "hasSwaStarted") {
    std::string swa_app_id;
    ASSERT_TRUE(value.GetString("swaAppId", &swa_app_id));

    *output = "false";

    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->InstanceRegistry().ForEachInstance(
        [&swa_app_id, &output](const apps::InstanceUpdate& update) {
          if (update.AppId() == swa_app_id &&
              update.State() & apps::InstanceState::kStarted) {
            *output = "true";
          }
        });

    return;
  }

  if (name == "countAppWindows") {
    std::string app_id;
    ASSERT_TRUE(value.GetString("appId", &app_id));

    const auto& app_windows =
        extensions::AppWindowRegistry::Get(profile())->app_windows();
    ASSERT_FALSE(app_windows.empty());
    int window_count = 0;
    for (auto* window : app_windows) {
      if (window->extension_id() == app_id)
        window_count++;
    }
    *output = base::NumberToString(window_count);
    return;
  }

  if (name == "runJsInAppWindow") {
    std::string window_id_str;
    ASSERT_TRUE(value.GetString("windowId", &window_id_str));
    int window_id = 0;
    ASSERT_TRUE(base::StringToInt(window_id_str, &window_id));
    std::string script;
    ASSERT_TRUE(value.GetString("script", &script));

    const auto& app_windows =
        extensions::AppWindowRegistry::Get(profile())->app_windows();
    ASSERT_FALSE(app_windows.empty());
    for (auto* window : app_windows) {
      CHECK(window);
      if (window->session_id().id() != window_id) {
        continue;
      }

      if (!window->web_contents())
        break;

      CHECK(window->web_contents()->GetMainFrame());
      window->web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
          base::UTF8ToUTF16(script), base::NullCallback());

      break;
    }
    return;
  }

  if (name == "enableTabletMode") {
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);
    *output = "tabletModeEnabled";
    return;
  }

  if (name == "runSelectFileDialog") {
    browser()->OpenFile();
    content::TestNavigationObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents(), 1);
    observer.Wait();
    *output = observer.last_navigation_url().spec();
    return;
  }

  if (name == "isSmbEnabled") {
    *output = options.native_smb ? "true" : "false";
    return;
  }

  if (name == "switchLanguage") {
    std::string language;
    ASSERT_TRUE(value.GetString("language", &language));
    base::RunLoop run_loop;
    chromeos::locale_util::SwitchLanguage(
        language, true, false,
        base::BindRepeating(
            [](base::RunLoop* run_loop,
               const chromeos::locale_util::LanguageSwitchResult&) {
              run_loop->Quit();
            },
            &run_loop),
        profile());
    run_loop.Run();
    return;
  }

  if (name == "runLauncherSearch") {
    app_list::LauncherSearchProvider search_provider(profile());
    base::string16 query;
    ASSERT_TRUE(value.GetString("query", &query));

    search_provider.Start(query);
    base::RunLoop run_loop;
    search_provider.set_result_changed_callback(run_loop.QuitClosure());
    run_loop.Run();

    std::vector<base::Value> names;
    for (auto& result : search_provider.results()) {
      names.emplace_back(result->title());
    }
    std::sort(names.begin(), names.end());
    base::JSONWriter::Write(base::Value(std::move(names)), output);
    return;
  }

  if (name == "blockFileTaskRunner") {
    BlockFileTaskRunner(profile());
    return;
  }

  if (name == "unblockFileTaskRunner") {
    UnblockFileTaskRunner();
    return;
  }

  if (name == "expectFileTask") {
    ExpectFileTasksMessage message;
    ASSERT_TRUE(ExpectFileTasksMessage::ConvertJSONValue(value, &message));
    // FileTasksNotifier is disabled in incognito or guest profiles.
    if (!file_tasks_observer_) {
      return;
    }
    for (const auto& file_name : message.file_names) {
      EXPECT_CALL(
          *file_tasks_observer_,
          OnFilesOpenedImpl(testing::HasSubstr(*file_name), message.open_type));
    }
    return;
  }

  if (name == "getHistogramCount") {
    GetHistogramCountMessage message;
    ASSERT_TRUE(GetHistogramCountMessage::ConvertJSONValue(value, &message));
    base::JSONWriter::Write(base::Value(histograms_.GetBucketCount(
                                message.histogram_name, message.value)),
                            output);

    return;
  }

  if (name == "getUserActionCount") {
    GetUserActionCountMessage message;
    ASSERT_TRUE(GetUserActionCountMessage::ConvertJSONValue(value, &message));
    base::JSONWriter::Write(
        base::Value(user_actions_.GetActionCount(message.user_action_name)),
        output);

    return;
  }

  if (name == "blockMounts") {
    chromeos::DBusThreadManager* dbus_thread_manager =
        chromeos::DBusThreadManager::Get();
    static_cast<chromeos::FakeCrosDisksClient*>(
        dbus_thread_manager->GetCrosDisksClient())
        ->BlockMount();
    return;
  }

  if (name == "setLastDownloadDir") {
    base::FilePath downloads_path(util::GetDownloadsMountPointName(profile()));
    downloads_path = downloads_path.AppendASCII("Downloads");
    auto* download_prefs = DownloadPrefs::FromBrowserContext(profile());
    download_prefs->SetSaveFilePath(downloads_path);
    return;
  }

  FAIL() << "Unknown test message: " << name;
}

drive::DriveIntegrationService*
FileManagerBrowserTestBase::CreateDriveIntegrationService(Profile* profile) {
  const Options options = GetOptions();
  drive_volumes_[profile->GetOriginalProfile()] =
      std::make_unique<DriveFsTestVolume>(profile->GetOriginalProfile());
  if (options.guest_mode != IN_INCOGNITO && options.mount_volumes &&
      profile->GetPath().BaseName().value() == "user") {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&LocalTestVolume::Mount),
                       base::Unretained(local_volume_.get()), profile));
  }
  if (!options.mount_volumes) {
    profile->GetPrefs()->SetBoolean(drive::prefs::kDriveFsPinnedMigrated, true);
  }
  auto* integration_service = drive_volumes_[profile->GetOriginalProfile()]
                                  ->CreateDriveIntegrationService(profile);
  if (!options.mount_volumes) {
    integration_service->SetEnabled(false);
  }
  return integration_service;
}

base::FilePath FileManagerBrowserTestBase::MaybeMountCrostini(
    const std::string& source_path,
    const std::vector<std::string>& mount_options) {
  GURL source_url(source_path);
  DCHECK(source_url.is_valid());
  if (source_url.scheme() != "sshfs") {
    return {};
  }
  CHECK(crostini_volume_->Mount(profile()));
  return crostini_volume_->mount_path();
}

void FileManagerBrowserTestBase::EnableVirtualKeyboard() {
  ash::ShellTestApi().EnableVirtualKeyboard();
}

}  // namespace file_manager
