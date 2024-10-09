// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_browsertest_base.h"

#include <stddef.h>

#include <compare>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string_view>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/webui/file_manager/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/immediate_crash.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_tags.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service.h"
#include "chrome/browser/ash/app_list/search/local_image_search/local_image_search_service_factory.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_media_view_util.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier.h"
#include "chrome/browser/ash/file_manager/file_tasks_notifier_factory.h"
#include "chrome/browser/ash/file_manager/file_tasks_observer.h"
#include "chrome/browser/ash/file_manager/mount_test_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/cloud_file_info.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/smb_client/smb_errors.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/ash/smb_client/smbfs_share.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service_factory.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/test_switches.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/cros_disks/fake_cros_disks_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mount_point.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-shared.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/smbfs/mojom/smbfs.mojom-shared.h"
#include "chromeos/ash/components/smbfs/mojom/smbfs.mojom.h"
#include "chromeos/ash/components/smbfs/smbfs_host.h"
#include "chromeos/ash/components/smbfs/smbfs_mounter.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "components/user_manager/user_manager.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/api/test/test_api_observer.h"
#include "extensions/browser/api/test/test_api_observer_registry.h"
#include "extensions/common/extension_id.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/base/network_change_notifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "services/network/public/mojom/network_change_manager.mojom-shared.h"
#include "storage/browser/file_system/copy_or_move_operation_delegate.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/input/input_event.mojom-shared.h"
#include "third_party/cros_system_api/dbus/cros-disks/dbus-constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace ash {
namespace smb_client {
class SmbUrl;
}  // namespace smb_client
}  // namespace ash
namespace content {
class BrowserContext;
}  // namespace content
namespace drivefs {
class DriveFsBootstrapListener;
}  // namespace drivefs
namespace extensions {
class Extension;
}  // namespace extensions
namespace guest_os {
class GuestOsFileWatcher;
}  // namespace guest_os

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

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

  content::RenderFrameHost* GetFrameHost() {
    return last_select_->GetPrimaryMainFrame();
  }

 private:
  scoped_refptr<SelectFileDialogExtension> last_select_;
};

namespace file_manager {
namespace {

// Waits `ash::locale_util::SwitchLanguage`.
class SwitchLanguageWaiter {
 public:
  ash::locale_util::SwitchLanguageCallback CreateCallback() {
    CHECK(!callback_created_)
        << "Only a single callback can be created for a waiter.";

    callback_created_ = true;
    return base::BindOnce(&SwitchLanguageWaiter::OnLanguageSwitch,
                          weak_ptr_factory_.GetWeakPtr());
  }

  void Wait() {
    CHECK(!run_loop_.running()) << "This waiter is already waiting.";
    run_loop_.Run();
  }

 private:
  void OnLanguageSwitch(const ash::locale_util::LanguageSwitchResult& result) {
    CHECK(result.success);
    CHECK_EQ(result.requested_locale, result.loaded_locale)
        << "Requested " << result.requested_locale << " but "
        << result.loaded_locale << " is loaded.";

    run_loop_.Quit();
  }

  bool callback_created_ = false;
  base::RunLoop run_loop_;

  base::WeakPtrFactory<SwitchLanguageWaiter> weak_ptr_factory_{this};
};

// Specialization of the navigation observer that stores web content every time
// the OnDidFinishNavigation is called.
class WebContentCapturingObserver : public content::TestNavigationObserver {
 public:
  explicit WebContentCapturingObserver(const GURL& url)
      : content::TestNavigationObserver(url) {}

  content::WebContents* web_contents() { return web_contents_; }

  void NavigationOfInterestDidFinish(
      content::NavigationHandle* navigation_handle) override {
    web_contents_ = navigation_handle->GetWebContents();
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
};

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
    MY_FILES,  // Same as Local Volume above.
    DRIVE_VOLUME,
    CROSTINI_VOLUME,
    GUEST_OS_VOLUME_0,  // GuestOS volume with provider id 0 (i.e. the first).
    USB_VOLUME,
    ANDROID_FILES_VOLUME,
    GENERIC_DOCUMENTS_PROVIDER_VOLUME,
    PHOTOS_DOCUMENTS_PROVIDER_VOLUME,
    MEDIA_VIEW_AUDIO,
    MEDIA_VIEW_IMAGES,
    MEDIA_VIEW_VIDEOS,
    MEDIA_VIEW_DOCUMENTS,
    SMBFS_VOLUME,
    MTP_VOLUME,
    PROVIDED_VOLUME,
  };

  // Represents the different types of entries (e.g. file, folder).
  enum EntryType { FILE, DIRECTORY, LINK, TEAM_DRIVE, COMPUTER };

  // Enumeration that determines the shared status of entries.
  enum SharedOption {
    // Not shared.
    NONE,

    // Shared but not visible in the 'Shared with me' view.
    SHARED,

    // Shared and appears in the 'Shared With Me' view.
    SHARED_WITH_ME,

    // Not directly shared, but belongs to a folder that is shared with me.
    // Entries marked as indirectly shared do not have the 'shared' metadata
    // field, and thus cannot be located via search for shared items.
    INDIRECTLY_SHARED_WITH_ME,
  };

  // The actual AddEntriesMessage contents.

  // The volume to add |entries| to.
  TargetVolume volume;

  // The |entries| to be added.
  std::vector<std::unique_ptr<struct TestEntryInfo>> entries;

  // Converts |value| to an AddEntriesMessage: true on success.
  static bool ConvertJSONValue(const base::Value::Dict& value,
                               AddEntriesMessage* message) {
    base::JSONValueConverter<AddEntriesMessage> converter;
    return converter.Convert(base::Value(value.Clone()), message);
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
  static bool MapStringToTargetVolume(std::string_view value,
                                      TargetVolume* volume) {
    if (value == "local") {
      *volume = LOCAL_VOLUME;
    } else if (value == "my_files") {
      *volume = MY_FILES;
    } else if (value == "drive") {
      *volume = DRIVE_VOLUME;
    } else if (value == "crostini") {
      *volume = CROSTINI_VOLUME;
    } else if (value == "guest_os_0") {
      *volume = GUEST_OS_VOLUME_0;
    } else if (value == "usb") {
      *volume = USB_VOLUME;
    } else if (value == "android_files") {
      *volume = ANDROID_FILES_VOLUME;
    } else if (value == "documents_provider") {
      *volume = GENERIC_DOCUMENTS_PROVIDER_VOLUME;
    } else if (value == "photos_documents_provider") {
      *volume = PHOTOS_DOCUMENTS_PROVIDER_VOLUME;
    } else if (value == "media_view_audio") {
      *volume = MEDIA_VIEW_AUDIO;
    } else if (value == "media_view_images") {
      *volume = MEDIA_VIEW_IMAGES;
    } else if (value == "media_view_videos") {
      *volume = MEDIA_VIEW_VIDEOS;
    } else if (value == "media_view_documents") {
      *volume = MEDIA_VIEW_DOCUMENTS;
    } else if (value == "provided") {
      *volume = PROVIDED_VOLUME;
    } else if (value == "smbfs") {
      *volume = SMBFS_VOLUME;
    } else if (value == "mtp") {
      *volume = MTP_VOLUME;
    } else {
      return false;
    }
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
    TestEntryInfo() : entry_type(FILE), shared_option(NONE) {}

    TestEntryInfo(EntryType entry_type,
                  const std::string& source_file_name,
                  const std::string& target_path)
        : entry_type(entry_type),
          shared_option(NONE),
          source_file_name(source_file_name),
          target_path(target_path),
          last_modified_time(base::Time::Now()) {}

    EntryType entry_type;             // Entry type: file or directory.
    SharedOption shared_option;       // File entry sharing option.
    std::string source_file_name;     // Source file name prototype.
    std::string thumbnail_file_name;  // DocumentsProvider thumbnail file name.
    std::string target_path;          // Target file or directory path.
    std::string name_text;            // Display file name.
    std::string team_drive_name;      // Name of team drive this entry is in.
    std::string computer_name;        // Name of the computer this entry is in.
    std::string mime_type;            // File entry content mime type.
    base::Time last_modified_time;    // Entry last modified time.
    EntryCapabilities capabilities;   // Entry permissions.
    EntryFolderFeature folder_feature;  // Entry folder feature.
    bool pinned = false;                // Whether the file should be pinned.
    bool dirty = false;                 // Whether the file is dirty.
    bool available_offline = false;  // Whether the file is available_offline.
    std::string alternate_url;       // Entry's alternate URL on Drive.
    bool can_pin = true;             // Whether the file can be pinned.

    TestEntryInfo& SetSharedOption(SharedOption option) {
      shared_option = option;
      return *this;
    }

    TestEntryInfo& SetThumbnailFileName(const std::string& file_name) {
      thumbnail_file_name = file_name;
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

    TestEntryInfo& SetDirty(bool is_dirty) {
      dirty = is_dirty;
      return *this;
    }

    TestEntryInfo& SetAvailableOffline(bool is_available_offline) {
      available_offline = is_available_offline;
      return *this;
    }

    TestEntryInfo& SetAlternateUrl(const std::string& new_alternate_url) {
      alternate_url = new_alternate_url;
      return *this;
    }

    // Registers the member information to the given converter.
    static void RegisterJSONConverter(
        base::JSONValueConverter<TestEntryInfo>* converter) {
      converter->RegisterCustomField("type", &TestEntryInfo::entry_type,
                                     &MapStringToEntryType);
      converter->RegisterStringField("sourceFileName",
                                     &TestEntryInfo::source_file_name);
      converter->RegisterStringField("thumbnailFileName",
                                     &TestEntryInfo::thumbnail_file_name);
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
      converter->RegisterBoolField("dirty", &TestEntryInfo::dirty);
      converter->RegisterBoolField("availableOffline",
                                   &TestEntryInfo::available_offline);
      converter->RegisterStringField("alternateUrl",
                                     &TestEntryInfo::alternate_url);
      converter->RegisterBoolField("canPin", &TestEntryInfo::can_pin);
    }

    // Maps |value| to an EntryType. Returns true on success.
    static bool MapStringToEntryType(std::string_view value, EntryType* type) {
      if (value == "file") {
        *type = FILE;
      } else if (value == "directory") {
        *type = DIRECTORY;
      } else if (value == "link") {
        *type = LINK;
      } else if (value == "team_drive") {
        *type = TEAM_DRIVE;
      } else if (value == "Computer") {
        *type = COMPUTER;
      } else {
        return false;
      }
      return true;
    }

    // Maps |value| to SharedOption. Returns true on success.
    static bool MapStringToSharedOption(std::string_view value,
                                        SharedOption* option) {
      if (value == "shared") {
        *option = SHARED;
      } else if (value == "sharedWithMe") {
        *option = SHARED_WITH_ME;
      } else if (value == "indirectlySharedWithMe") {
        *option = INDIRECTLY_SHARED_WITH_ME;
      } else if (value == "none") {
        *option = NONE;
      } else {
        return false;
      }
      return true;
    }

    // Maps |value| to base::Time. Returns true on success.
    static bool MapStringToTime(std::string_view value, base::Time* time) {
      return base::Time::FromString(std::string(value).c_str(), time);
    }
  };
};

// Listens for chrome.test messages: PASS, FAIL, and SendMessage.
class FileManagerTestMessageListener : public extensions::TestApiObserver {
 public:
  struct Message {
    enum class Completion {
      kNone,
      kPass,
      kFail,
    };

    Completion completion;
    std::string message;
    scoped_refptr<extensions::TestSendMessageFunction> function;
  };

  FileManagerTestMessageListener() {
    test_api_observation_.Observe(
        extensions::TestApiObserverRegistry::GetInstance());
  }

  FileManagerTestMessageListener(const FileManagerTestMessageListener&) =
      delete;
  FileManagerTestMessageListener& operator=(
      const FileManagerTestMessageListener&) = delete;

  ~FileManagerTestMessageListener() override = default;

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

 private:
  // extensions::TestApiObserver:
  void OnTestPassed(content::BrowserContext* browser_context) override {
    test_complete_ = true;
    QueueMessage({Message::Completion::kPass, std::string(), nullptr});
  }
  void OnTestFailed(content::BrowserContext* browser_context,
                    const std::string& message) override {
    test_complete_ = true;
    QueueMessage({Message::Completion::kFail, message, nullptr});
  }
  bool OnTestMessage(extensions::TestSendMessageFunction* function,
                     const std::string& message) override {
    // crbug.com/668680
    EXPECT_FALSE(test_complete_) << "LATE MESSAGE: " << message;
    QueueMessage({Message::Completion::kNone, message, function});
    return true;
  }

  void QueueMessage(const Message& message) {
    messages_.push_back(message);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  bool test_complete_ = false;
  base::OnceClosure quit_closure_;
  base::circular_deque<Message> messages_;
  base::ScopedObservation<extensions::TestApiObserverRegistry,
                          extensions::TestApiObserver>
      test_api_observation_{this};
};

// Test volume.
class TestVolume {
 protected:
  explicit TestVolume(const std::string& name) : name_(name) {}

  TestVolume(const TestVolume&) = delete;
  TestVolume& operator=(const TestVolume&) = delete;

  virtual ~TestVolume() = default;

  bool CreateRootDirectory(const Profile* profile) {
    if (root_initialized_) {
      return true;
    }
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
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
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

  profile->GetDefaultStoragePartition()
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
  static bool ConvertJSONValue(const base::Value::Dict& value,
                               ExpectFileTasksMessage* message) {
    base::JSONValueConverter<ExpectFileTasksMessage> converter;
    return converter.Convert(base::Value(value.Clone()), message);
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<ExpectFileTasksMessage>* converter) {
    converter->RegisterCustomField(
        "openType", &ExpectFileTasksMessage::open_type, &MapStringToOpenType);
    converter->RegisterRepeatedString("fileNames",
                                      &ExpectFileTasksMessage::file_names);
  }

  static bool MapStringToOpenType(
      std::string_view value,
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
  static bool ConvertJSONValue(const base::Value::Dict& value,
                               GetHistogramCountMessage* message) {
    base::JSONValueConverter<GetHistogramCountMessage> converter;
    return converter.Convert(base::Value(value.Clone()), message);
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

struct GetTotalHistogramSum {
  static bool ConvertJSONValue(const base::Value::Dict& value,
                               GetTotalHistogramSum* message) {
    base::JSONValueConverter<GetTotalHistogramSum> converter;
    return converter.Convert(base::Value(value.Clone()), message);
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<GetTotalHistogramSum>* converter) {
    converter->RegisterStringField("histogramName",
                                   &GetTotalHistogramSum::histogram_name);
  }

  std::string histogram_name;
};

struct ExpectHistogramTotalCountMessage {
  static bool ConvertJSONValue(const base::Value::Dict& value,
                               ExpectHistogramTotalCountMessage* message) {
    base::JSONValueConverter<ExpectHistogramTotalCountMessage> converter;
    return converter.Convert(base::Value(value.Clone()), message);
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<ExpectHistogramTotalCountMessage>* converter) {
    converter->RegisterStringField(
        "histogramName", &ExpectHistogramTotalCountMessage::histogram_name);
    converter->RegisterIntField("count",
                                &ExpectHistogramTotalCountMessage::count);
  }

  std::string histogram_name;
  int count = 0;
};

struct GetUserActionCountMessage {
  static bool ConvertJSONValue(const base::Value::Dict& value,
                               GetUserActionCountMessage* message) {
    base::JSONValueConverter<GetUserActionCountMessage> converter;
    return converter.Convert(base::Value(value.Clone()), message);
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<GetUserActionCountMessage>* converter) {
    converter->RegisterStringField(
        "userActionName", &GetUserActionCountMessage::user_action_name);
  }

  std::string user_action_name;
};

struct GetLocalPathMessage {
  static bool ConvertJSONValue(const base::Value::Dict& value,
                               GetLocalPathMessage* message) {
    base::JSONValueConverter<GetLocalPathMessage> converter;
    return converter.Convert(base::Value(value.Clone()), message);
  }

  static void RegisterJSONConverter(
      base::JSONValueConverter<GetLocalPathMessage>* converter) {
    converter->RegisterStringField("localPath",
                                   &GetLocalPathMessage::local_path);
  }

  std::string local_path;
};

views::Widget* FindSharesheetWidget() {
  for (aura::Window* root_window : ash::Shell::GetAllRootWindows()) {
    views::Widget::Widgets widgets;
    views::Widget::GetAllChildWidgets(root_window, &widgets);
    for (views::Widget* widget : widgets) {
      if (widget->GetName() == "SharesheetBubbleView") {
        return widget;
      }
    }
  }
  return nullptr;
}

}  // anonymous namespace

ash::LoggedInUserMixin::LogInType LogInTypeFor(
    TestAccountType test_account_type) {
  switch (test_account_type) {
    case kTestAccountTypeNotSet:
      CHECK(false) << "test_account_type option must be set for "
                      "LoggedInUserFilesAppBrowserTest";
      // TODO(crbug.com/40122554): `base::ImmediateCrash` is necessary.
      base::ImmediateCrash();
    case kEnterprise:
    case kGoogler:
      return ash::LoggedInUserMixin::LogInType::kManaged;
    case kChild:
      return ash::LoggedInUserMixin::LogInType::kChild;
    case kNonManaged:
    case kNonManagedNonOwner:
      return ash::LoggedInUserMixin::LogInType::kConsumer;
  }
}

std::optional<AccountId> AccountIdFor(TestAccountType test_account_type) {
  switch (test_account_type) {
    case kTestAccountTypeNotSet:
      CHECK(false) << "test_account_type option must be set for "
                      "LoggedInUserFilesAppBrowserTest";
      // `base::ImmediateCrash` is necessary for https://crbug.com/1061742.
      base::ImmediateCrash();
    case kGoogler:
      return AccountId::FromUserEmailGaiaId(
          "user@google.com", FakeGaiaMixin::kEnterpriseUser1GaiaId);
    case kChild:
    case kEnterprise:
    case kNonManaged:
    case kNonManagedNonOwner:
      // Use the default account provided by `LoggedInUserMixin`.
      return std::nullopt;
  }
}

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
FileManagerBrowserTestBase::Options::~Options() = default;

std::ostream& operator<<(std::ostream& out,
                         const FileManagerBrowserTestBase::Options& options) {
  out << "{";

  // Don't print separator before first member.
  auto sep = [i = 0]() mutable { return i++ ? ", " : ""; };

  // Only print members with non-default values.
  const FileManagerBrowserTestBase::Options defaults;

  // Print guest mode first, followed by boolean members in lexicographic order.
  if (options.guest_mode != defaults.guest_mode) {
    out << sep() << options.guest_mode;
  }

#define PRINT_IF_NOT_DEFAULT(N) \
  if (options.N != defaults.N)  \
    out << sep() << (options.N ? "" : "!") << #N;

  PRINT_IF_NOT_DEFAULT(arc)
  PRINT_IF_NOT_DEFAULT(browser)
  PRINT_IF_NOT_DEFAULT(generic_documents_provider)
  PRINT_IF_NOT_DEFAULT(mount_volumes)
  PRINT_IF_NOT_DEFAULT(native_smb)
  PRINT_IF_NOT_DEFAULT(offline)
  PRINT_IF_NOT_DEFAULT(photos_documents_provider)
  PRINT_IF_NOT_DEFAULT(single_partition_format)
  PRINT_IF_NOT_DEFAULT(tablet_mode)
  PRINT_IF_NOT_DEFAULT(enable_arc_vm)

#undef PRINT_IF_NOT_DEFAULT

  return out << "}";
}

class FileManagerBrowserTestBase::MockFileTasksObserver
    : public file_tasks::FileTasksObserver {
 public:
  explicit MockFileTasksObserver(Profile* profile) {
    observation_.Observe(
        file_tasks::FileTasksNotifierFactory::GetForProfile(profile));
  }

  MOCK_METHOD2(OnFilesOpenedImpl,
               void(const std::string& path, OpenType open_type));

  void OnFilesOpened(const std::vector<FileOpenEvent>& opens) override {
    ASSERT_TRUE(!opens.empty());
    for (auto& open : opens) {
      OnFilesOpenedImpl(open.path.value(), open.open_type);
    }
  }

 private:
  base::ScopedObservation<file_tasks::FileTasksNotifier,
                          file_tasks::FileTasksObserver>
      observation_{this};
};

// LocalTestVolume: test volume for a local drive.
class LocalTestVolume : public TestVolume {
 public:
  explicit LocalTestVolume(const std::string& name) : TestVolume(name) {}

  LocalTestVolume(const LocalTestVolume&) = delete;
  LocalTestVolume& operator=(const LocalTestVolume&) = delete;

  ~LocalTestVolume() override = default;

  // Adds this local volume. Returns true on success.
  virtual bool Mount(Profile* profile) = 0;

  virtual void CreateEntry(const AddEntriesMessage::TestEntryInfo& entry) {
    CreateEntryImpl(entry, root_path().AppendASCII(entry.target_path));
  }

  void InsertEntryOnMap(const AddEntriesMessage::TestEntryInfo& entry,
                        const base::FilePath& target_path) {
    const auto it = entries_.find(target_path);
    if (it == entries_.end()) {
      entries_.insert(std::make_pair(target_path, entry));
    }
  }

  void CreateEntryImpl(const AddEntriesMessage::TestEntryInfo& entry,
                       const base::FilePath& target_path) {
    entries_.insert(std::make_pair(target_path, entry));
    switch (entry.entry_type) {
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
        NOTREACHED_IN_MIGRATION()
            << "Can't create a team drive in a local volume: "
            << target_path.value();
        break;
      case AddEntriesMessage::COMPUTER:
        NOTREACHED_IN_MIGRATION()
            << "Can't create a computer in a local volume: "
            << target_path.value();
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unsupported entry type for: " << target_path.value();
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
      if (it == entries_.end()) {
        return false;
      }
      return UpdateModifiedTime(it->second, path.DirName());
    }

    return true;
  }

  std::map<base::FilePath, const AddEntriesMessage::TestEntryInfo> entries_;
};

// DownloadsTestVolume: local test volume for the "Downloads" directory.
class DownloadsTestVolume : public LocalTestVolume {
 public:
  DownloadsTestVolume() : LocalTestVolume("MyFiles") {}

  DownloadsTestVolume(const DownloadsTestVolume&) = delete;
  DownloadsTestVolume& operator=(const DownloadsTestVolume&) = delete;

  ~DownloadsTestVolume() override = default;

  void EnsureDownloadsFolderExists() {
    // When MyFiles is the volume create the Downloads folder under it.
    auto downloads_folder = root_path().Append("Downloads");
    auto downloads_entry = AddEntriesMessage::TestEntryInfo(
        AddEntriesMessage::DIRECTORY, "", "Downloads");
    if (!base::PathExists(downloads_folder)) {
      CreateEntryImpl(downloads_entry, downloads_folder);
    }

    // Make sure that Downloads exists in the local entries_ map, in case the
    // folder in the FS has been created by a PRE_ routine.
    InsertEntryOnMap(downloads_entry, downloads_folder);
  }
  // Forces the content to be created inside MyFiles/Downloads when MyFiles is
  // the Volume, so tests are compatible with volume being MyFiles or Downloads.
  // TODO(lucmult): Remove this special case once MyFiles volume has been
  // rolled out.
  base::FilePath base_path() const { return root_path().Append("Downloads"); }

  base::FilePath GetFilePath(const std::string relative_path) const {
    return base_path().Append(relative_path);
  }

  bool Mount(Profile* profile) override {
    if (!CreateRootDirectory(profile)) {
      return false;
    }
    EnsureDownloadsFolderExists();
    auto* volume = VolumeManager::Get(profile);
    return volume->RegisterDownloadsDirectoryForTesting(root_path());
  }

  void CreateEntry(const AddEntriesMessage::TestEntryInfo& entry) override {
    base::FilePath target_path = GetFilePath(entry.target_path);
    CreateEntryImpl(entry, target_path);
  }

  void CreateEntryAtRoot(const AddEntriesMessage::TestEntryInfo& entry) {
    base::FilePath target_path = root_path().Append(entry.target_path);
    CreateEntryImpl(entry, target_path);
  }

  void Unmount(Profile* profile) {
    auto* volume = VolumeManager::Get(profile);
    volume->RemoveDownloadsDirectoryForTesting();
  }
};

class AndroidFilesTestVolume : public LocalTestVolume {
 public:
  AndroidFilesTestVolume() : LocalTestVolume("AndroidFiles") {}

  AndroidFilesTestVolume(const AndroidFilesTestVolume&) = delete;
  AndroidFilesTestVolume& operator=(const AndroidFilesTestVolume&) = delete;

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
};

// CrostiniTestVolume: local test volume for the "Linux files" directory.
class CrostiniTestVolume : public LocalTestVolume {
 public:
  explicit CrostiniTestVolume(const std::string& source_path)
      : LocalTestVolume("Crostini"), source_path_(source_path) {}

  CrostiniTestVolume(const CrostiniTestVolume&) = delete;
  CrostiniTestVolume& operator=(const CrostiniTestVolume&) = delete;

  ~CrostiniTestVolume() override = default;

  // Create root dir so entries can be created, but volume is not mounted.
  bool Initialize(Profile* profile) { return CreateRootDirectory(profile); }

  bool Mount(Profile* profile) override {
    return CreateRootDirectory(profile) &&
           VolumeManager::Get(profile)->RegisterCrostiniDirectoryForTesting(
               root_path());
  }

  const base::FilePath& mount_path() const { return root_path(); }

  const std::string& source_path() const { return source_path_; }

 private:
  std::string source_path_;
};

// FakeTestVolume: local test volume with a given volume and device type.
class FakeTestVolume : public LocalTestVolume {
 public:
  FakeTestVolume(const std::string& name,
                 VolumeType volume_type,
                 ash::DeviceType device_type)
      : LocalTestVolume(name),
        volume_type_(volume_type),
        device_type_(device_type) {}

  FakeTestVolume(const FakeTestVolume&) = delete;
  FakeTestVolume& operator=(const FakeTestVolume&) = delete;

  ~FakeTestVolume() override = default;

  // Add the fake test volume entries.
  bool PrepareTestEntries(Profile* profile) {
    if (!CreateRootDirectory(profile)) {
      return false;
    }

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
    if (!CreateRootDirectory(profile)) {
      return false;
    }

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
    if (!MountSetup(profile)) {
      return false;
    }

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
    if (!CreateRootDirectory(profile)) {
      return false;
    }

    // Revoke name() mount point first, then re-add its mount point.
    GetMountPoints()->RevokeFileSystem(name());
    const bool added = GetMountPoints()->RegisterFileSystem(
        name(), storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        root_path());
    if (!added) {
      return false;
    }

    return true;
  }

  const VolumeType volume_type_;
  const ash::DeviceType device_type_;
  const bool read_only_ = false;
};

// Removable TestVolume: local test volume for external media devices.
class RemovableTestVolume : public FakeTestVolume {
 public:
  RemovableTestVolume(const std::string& name,
                      VolumeType volume_type,
                      ash::DeviceType device_type,
                      const base::FilePath& device_path,
                      const std::string& drive_label,
                      const std::string& file_system_type)
      : FakeTestVolume(name, volume_type, device_type),
        device_path_(device_path),
        drive_label_(drive_label),
        file_system_type_(file_system_type) {}

  RemovableTestVolume(const RemovableTestVolume&) = delete;
  RemovableTestVolume& operator=(const RemovableTestVolume&) = delete;

  ~RemovableTestVolume() override = default;

  bool Mount(Profile* profile) override {
    if (!MountSetup(profile)) {
      return false;
    }

    // Expose the mount point with the given volume and device type.
    VolumeManager::Get(profile)->AddVolumeForTesting(
        root_path(), volume_type_, device_type_, read_only_, device_path_,
        drive_label_, file_system_type_, /*hidden=*/false, /*watchable=*/true);
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
};

// DriveFsTestVolume: test volume for Google Drive using DriveFS.
class DriveFsTestVolume : public TestVolume {
 public:
  explicit DriveFsTestVolume(Profile* original_profile)
      : TestVolume("drive"), original_profile_(original_profile) {}

  DriveFsTestVolume(const DriveFsTestVolume&) = delete;
  DriveFsTestVolume& operator=(const DriveFsTestVolume&) = delete;

  ~DriveFsTestVolume() override = default;

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    if (!CreateRootDirectory(profile)) {
      return nullptr;
    }

    EXPECT_FALSE(profile_);
    profile_ = profile;

    EXPECT_FALSE(integration_service_);
    integration_service_ = new drive::DriveIntegrationService(
        profile, std::string(), root_path().Append("v1"),
        CreateDriveFsBootstrapListener());

    return integration_service_;
  }

  bool Mount(Profile* profile) {
    if (profile != profile_) {
      return false;
    }

    if (!integration_service_) {
      return false;
    }

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
    switch (entry.entry_type) {
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
    drivefs::FakeMetadata metadata;
    metadata.path = relative_path;
    metadata.mime_type = entry.mime_type;
    metadata.original_name = original_name.value();
    metadata.dirty = entry.dirty;
    metadata.pinned = entry.pinned;
    metadata.available_offline = entry.available_offline;
    metadata.shared =
        (entry.shared_option == AddEntriesMessage::SharedOption::SHARED ||
         entry.shared_option ==
             AddEntriesMessage::SharedOption::SHARED_WITH_ME);
    metadata.capabilities.can_share = entry.capabilities.can_share;
    metadata.capabilities.can_copy = entry.capabilities.can_copy;
    metadata.capabilities.can_delete = entry.capabilities.can_delete;
    metadata.capabilities.can_rename = entry.capabilities.can_rename,
    metadata.capabilities.can_add_children =
        entry.capabilities.can_add_children;
    metadata.folder_feature.is_machine_root =
        entry.folder_feature.is_machine_root;
    metadata.folder_feature.is_arbitrary_sync_folder =
        entry.folder_feature.is_arbitrary_sync_folder;
    metadata.folder_feature.is_external_media =
        entry.folder_feature.is_external_media;
    metadata.alternate_url = entry.alternate_url;
    if (entry.entry_type == AddEntriesMessage::LINK) {
      metadata.shortcut = true;
      metadata.shortcut_target_path = target_path;
    }
    metadata.can_pin = entry.can_pin;
    fake_drivefs_helper_->fake_drivefs().SetMetadata(std::move(metadata));

    ASSERT_TRUE(UpdateModifiedTime(entry));
  }

  void DisplayConfirmDialog(drivefs::mojom::DialogReasonPtr reason) {
    fake_drivefs_helper_->fake_drivefs().DisplayConfirmDialog(
        std::move(reason), base::BindOnce(&DriveFsTestVolume::OnDialogResult,
                                          base::Unretained(this)));
  }

  void SetFileSyncStatus(const std::string* path,
                         const drivefs::mojom::ItemEvent::State sync_status,
                         const drivefs::mojom::ItemEventReason reason,
                         int64_t bytes_transferred,
                         int64_t bytes_to_transfer) {
    const base::FilePath file_path(*path);
    const auto& md =
        fake_drivefs_helper_->fake_drivefs().GetItemMetadata(file_path);
    CHECK(md.has_value()) << "No metadata found for " << file_path.value();

    drivefs::mojom::SyncingStatus syncing_status;
    drivefs::mojom::ItemEventPtr event = drivefs::mojom::ItemEvent::New();
    event->stable_id = md.value().stable_id;
    event->group_id = 1;
    event->path = *path;
    event->state = sync_status;
    event->bytes_transferred = bytes_transferred;
    event->bytes_to_transfer = bytes_to_transfer;
    event->reason = reason;
    event->is_download = (reason == drivefs::mojom::ItemEventReason::kPin);
    LOG(ERROR) << "Sending sync status event for: " << event->stable_id << " : "
               << event->path;
    syncing_status.item_events.push_back(std::move(event));

    auto& drivefs_delegate = fake_drivefs_helper_->fake_drivefs().delegate();
    drivefs_delegate->OnSyncingStatusUpdate(syncing_status.Clone());
    drivefs_delegate.FlushForTesting();
  }

  void SetFileProgress(const std::string* path, const int progress) {
    const base::FilePath file_path(*path);
    const auto& md =
        fake_drivefs_helper_->fake_drivefs().GetItemMetadata(file_path);
    CHECK(md.has_value()) << "No metadata found for " << file_path.value();

    auto progress_event = drivefs::mojom::ProgressEvent::New();
    base::FilePath full_path = mount_path();
    CHECK(base::FilePath("/").AppendRelativePath(base::FilePath(*path),
                                                 &full_path))
        << "Failed to convert to full path";
    progress_event->file_path = full_path;
    progress_event->progress = progress;
    progress_event->stable_id = md.value().stable_id;

    auto& drivefs_delegate = fake_drivefs_helper_->fake_drivefs().delegate();
    drivefs_delegate->OnItemProgress(std::move(progress_event));
    drivefs_delegate.FlushForTesting();
  }

  void SetSyncError(const std::string* path) {
    const base::FilePath file_path(*path);
    const auto& md =
        fake_drivefs_helper_->fake_drivefs().GetItemMetadata(file_path);
    CHECK(md.has_value()) << "No metadata found for " << file_path.value();

    auto drive_error = drivefs::mojom::DriveError::New();
    drive_error->path = base::FilePath(*path);
    drive_error->stable_id = md.value().stable_id;

    auto& drivefs_delegate = fake_drivefs_helper_->fake_drivefs().delegate();
    drivefs_delegate->OnError(std::move(drive_error));
    drivefs_delegate.FlushForTesting();
  }

  void SendCloudDeleteEvent(const std::string& path) {
    const base::FilePath file_path(path);
    std::optional<drivefs::FakeDriveFs::FileMetadata> metadata =
        fake_drivefs_helper_->fake_drivefs().GetItemMetadata(file_path);
    ASSERT_TRUE(metadata.has_value()) << "No file metadata with path: " << path;

    std::vector<drivefs::mojom::FileChangePtr> file_changes;
    file_changes.emplace_back(std::in_place, file_path,
                              drivefs::mojom::FileChange::Type::kDelete,
                              metadata.value().stable_id);
    auto& drivefs_delegate = fake_drivefs_helper_->fake_drivefs().delegate();
    drivefs_delegate->OnFilesChanged(std::move(file_changes));
    drivefs_delegate.FlushForTesting();
  }

  std::optional<drivefs::mojom::DialogResult> last_dialog_result() {
    return last_dialog_result_;
  }

  std::optional<bool> IsItemPinned(const std::string& path) {
    return fake_drivefs_helper_->fake_drivefs().IsItemPinned(path);
  }

  void SetCanPin(const std::string& path, bool can_pin) {
    ASSERT_TRUE(fake_drivefs_helper_->fake_drivefs().SetCanPin(path, can_pin));

    const base::FilePath file_path(path);
    std::optional<drivefs::FakeDriveFs::FileMetadata> metadata =
        fake_drivefs_helper_->fake_drivefs().GetItemMetadata(file_path);
    ASSERT_TRUE(metadata.has_value()) << "No file metadata with path: " << path;

    std::vector<drivefs::mojom::FileChangePtr> file_changes;
    file_changes.emplace_back(std::in_place, file_path,
                              drivefs::mojom::FileChange::Type::kModify,
                              metadata.value().stable_id);
    auto& drivefs_delegate = fake_drivefs_helper_->fake_drivefs().delegate();
    drivefs_delegate->OnFilesChanged(std::move(file_changes));
  }

  void SetPooledStorageQuotaUsage(int64_t used_user_bytes,
                                  int64_t total_user_bytes,
                                  bool organization_limit_exceeded) {
    fake_drivefs_helper_->fake_drivefs().SetPooledStorageQuotaUsage(
        used_user_bytes, total_user_bytes, organization_limit_exceeded);
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
      if (it == entries_.end()) {
        return false;
      }
      return UpdateModifiedTime(it->second);
    }

    return true;
  }

  base::FilePath GetTargetPathForTestEntry(
      const AddEntriesMessage::TestEntryInfo& entry) {
    const base::FilePath target_path =
        GetTargetBasePathForTestEntry(entry).Append(entry.target_path);
    if (entry.name_text != entry.target_path) {
      return target_path.DirName().Append(entry.name_text);
    }
    return target_path;
  }

  base::FilePath GetTargetBasePathForTestEntry(
      const AddEntriesMessage::TestEntryInfo& entry) {
    if (entry.shared_option == AddEntriesMessage::SHARED_WITH_ME ||
        entry.shared_option == AddEntriesMessage::INDIRECTLY_SHARED_WITH_ME) {
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

  void OnDialogResult(drivefs::mojom::DialogResult result) {
    last_dialog_result_ = result;
  }

  std::optional<drivefs::mojom::DialogResult> last_dialog_result_;

  // Profile associated with this volume: not owned.
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  // Integration service used for testing: not owned.
  raw_ptr<drive::DriveIntegrationService, DanglingUntriaged>
      integration_service_ = nullptr;

  const raw_ptr<Profile, DanglingUntriaged> original_profile_;
  std::map<base::FilePath, const AddEntriesMessage::TestEntryInfo> entries_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
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

  DocumentsProviderTestVolume(const DocumentsProviderTestVolume&) = delete;
  DocumentsProviderTestVolume& operator=(const DocumentsProviderTestVolume&) =
      delete;

  ~DocumentsProviderTestVolume() override = default;

  virtual void CreateEntry(const AddEntriesMessage::TestEntryInfo& entry) {
    // Create and add an entry Document to the fake arc::FileSystemInstance.
    arc::FakeFileSystemInstance::Document document(
        authority_, entry.name_text, root_document_id_, entry.name_text,
        GetMimeType(entry), GetFileSize(entry),
        entry.last_modified_time.InMillisecondsSinceUnixEpoch(),
        entry.capabilities.can_delete, entry.capabilities.can_rename,
        entry.capabilities.can_add_children,
        !entry.thumbnail_file_name.empty());
    file_system_instance_->AddDocument(document);

    if (entry.entry_type != AddEntriesMessage::FILE) {
      return;
    }

    // arc::FakeFileSystemInstance has a dedicated method AddRecentDocument(),
    // to make the newly added file entry work with Recents view, we need to
    // manually call that method to add the new entry to recent file list.
    base::Time cutoff_time = base::Time::Now() - base::Days(30);
    if (entry.last_modified_time > cutoff_time) {
      file_system_instance_->AddRecentDocument(root_document_id_, document);
    }

    std::string canonical_url = base::StrCat(
        {"content://", authority_, "/document/", EncodeURI(entry.name_text)});
    arc::FakeFileSystemInstance::File file(
        canonical_url, GetTestFileContent(entry.source_file_name),
        GetMimeType(entry), arc::FakeFileSystemInstance::File::Seekable::NO);
    if (!entry.thumbnail_file_name.empty()) {
      file.thumbnail_content = GetTestFileContent(entry.thumbnail_file_name);
    }
    file_system_instance_->AddFile(file);
  }

  virtual bool Mount(Profile* profile) {
    // Register the volume root document.
    RegisterRoot();

    // Tell VolumeManager that a new DocumentsProvider volume is added.
    VolumeManager::Get(profile)->OnDocumentsProviderRootAdded(
        authority_, root_document_id_, root_document_id_, name(), "", GURL(),
        read_only_, std::vector<std::string>());
    return true;
  }

 protected:
  const raw_ptr<arc::FakeFileSystemInstance, DanglingUntriaged>
      file_system_instance_;
  const std::string authority_;
  const std::string root_document_id_;
  const bool read_only_;

  void RegisterRoot() {
    const auto* root_mime_type = arc::kAndroidDirectoryMimeType;
    file_system_instance_->AddDocument(arc::FakeFileSystemInstance::Document(
        authority_, root_document_id_, "", "", root_mime_type, 0, 0));
  }

 private:
  int64_t GetFileSize(const AddEntriesMessage::TestEntryInfo& entry) {
    if (entry.entry_type != AddEntriesMessage::FILE) {
      return 0;
    }

    int64_t file_size = 0;
    const base::FilePath source_path =
        TestVolume::GetTestDataFilePath(entry.source_file_name);
    bool success = base::GetFileSize(source_path, &file_size);
    return success ? file_size : 0;
  }

  std::string GetMimeType(const AddEntriesMessage::TestEntryInfo& entry) {
    return entry.entry_type == AddEntriesMessage::FILE
               ? entry.mime_type
               : arc::kAndroidDirectoryMimeType;
  }

  std::string GetTestFileContent(const std::string& test_file_name) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string contents;
    base::FilePath path = TestVolume::GetTestDataFilePath(test_file_name);
    CHECK(base::ReadFileToString(path, &contents))
        << "failed reading test data file " << test_file_name;
    return contents;
  }

  std::string EncodeURI(const std::string& component) {
    url::RawCanonOutputT<char> encoded;
    url::EncodeURIComponent(component, &encoded);
    return std::string(encoded.view());
  }
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

  MediaViewTestVolume(const MediaViewTestVolume&) = delete;
  MediaViewTestVolume& operator=(const MediaViewTestVolume&) = delete;

  ~MediaViewTestVolume() override = default;

  bool Mount(Profile* profile) override {
    RegisterRoot();
    return VolumeManager::Get(profile)->RegisterMediaViewForTesting(
        root_document_id_);
  }
};

using ash::file_system_provider::Capabilities;
using ash::file_system_provider::FakeExtensionProvider;
using ash::file_system_provider::FakeProvidedFileSystem;
using ash::file_system_provider::MountOptions;
using ash::file_system_provider::ProvidedFileSystemInfo;

// An extension provider that customizes the FakeExtensionProvider. The
// FakeExtensionProvider creates an unwatchable volume, which is not suitable
// for tests. Thus we expose the constructor to allow custom capabilities to be
// passed.
class TestExtensionProvider : public FakeExtensionProvider {
 public:
  TestExtensionProvider(const extensions::ExtensionId& extension_id,
                        const Capabilities& capabilities)
      : FakeExtensionProvider(extension_id, capabilities) {}
};

// Creates a fake file system provider. To use it in your test please add
// .FakeFileSystemProvider() option in your test declaration.
class FileSystemProviderTestVolume : public TestVolume {
 public:
  FileSystemProviderTestVolume()
      : TestVolume("provided"),
        extension_id_("test-file-system-provider-id"),
        provider_id_(
            ash::file_system_provider::ProviderId::CreateFromExtensionId(
                extension_id_)) {}

  FileSystemProviderTestVolume(const FileSystemProviderTestVolume&) = delete;
  FileSystemProviderTestVolume& operator=(const FileSystemProviderTestVolume&) =
      delete;

  ~FileSystemProviderTestVolume() override = default;

  void Mount(Profile* profile) {
    // In order for the test file system provider volume to be correctly mounted
    // we need to register a provider (ProviderInterface) with the file system
    // provider service. We use a customized FakeExtensionProvider, which has
    // a factory method that builds an instance of the FakeProvidedFileSystem
    // which is a ProvidedFileSystemInterface. That instance is what does the
    // creation of entries, reading of directories, etc.
    Capabilities capabilities = {
        .configurable = false,
        .watchable = true,
        .multiple_mounts = false,
        .source = extensions::SOURCE_NETWORK,
    };
    std::unique_ptr<ash::file_system_provider::ProviderInterface> provider =
        std::make_unique<TestExtensionProvider>(extension_id_, capabilities);
    ash::file_system_provider::Service* service =
        ash::file_system_provider::Service::Get(profile);
    service->RegisterProvider(std::move(provider));

    MountOptions options("test-fsp", "TestFSP");
    EXPECT_EQ(base::File::FILE_OK,
              service->MountFileSystem(provider_id_, options));
  }

  void CreateEntry(Profile* profile,
                   const AddEntriesMessage::TestEntryInfo& entry) {
    ash::file_system_provider::Service* service =
        ash::file_system_provider::Service::Get(profile);
    DCHECK(service) << "Unable to retrieve file system provider service";
    std::vector<ash::file_system_provider::ProvidedFileSystemInfo>
        file_systems = service->GetProvidedFileSystemInfoList(provider_id_);
    DCHECK(file_systems.size() == 1)
        << "Unexpected number " << file_systems.size()
        << " of file systems for provider_id " << provider_id_.ToString();
    FakeProvidedFileSystem* fake_file_system =
        static_cast<FakeProvidedFileSystem*>(service->GetProvidedFileSystem(
            provider_id_, file_systems[0].file_system_id()));
    DCHECK(fake_file_system)
        << "Unable to get fake file system for provider_id_ "
        << provider_id_.ToString();
    bool folder = entry.entry_type == AddEntriesMessage::EntryType::DIRECTORY;
    std::string file_contents = folder ? "" : "abcdef";
    fake_file_system->AddEntry(base::FilePath(entry.target_path), folder,
                               entry.name_text, file_contents.length(),
                               entry.last_modified_time, entry.mime_type,
                               /*cloud_file_info=*/nullptr, file_contents);
  }

 private:
  extensions::ExtensionId extension_id_;
  ash::file_system_provider::ProviderId provider_id_;
};

// An internal volume which is hidden from file manager.
class HiddenTestVolume : public FakeTestVolume {
 public:
  HiddenTestVolume()
      : FakeTestVolume("internal_test",
                       VolumeType::VOLUME_TYPE_SYSTEM_INTERNAL,
                       ash::DeviceType::kUnknown) {}
  HiddenTestVolume(const HiddenTestVolume&) = delete;
  HiddenTestVolume& operator=(const HiddenTestVolume&) = delete;

  bool Mount(Profile* profile) override {
    if (!MountSetup(profile)) {
      return false;
    }

    // Expose the mount point with the given volume and device type.
    VolumeManager::Get(profile)->AddVolumeForTesting(
        root_path(), volume_type_, device_type_, read_only_,
        /*device_path=*/base::FilePath(),
        /*drive_label=*/"", /*file_system_type=*/"", /*hidden=*/true);
    base::RunLoop().RunUntilIdle();
    return true;
  }
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

  SmbfsTestVolume(const SmbfsTestVolume&) = delete;
  SmbfsTestVolume& operator=(const SmbfsTestVolume&) = delete;

  ~SmbfsTestVolume() override = default;

  // Create root dir so entries can be created, but volume is not mounted.
  bool Initialize(Profile* profile) { return CreateRootDirectory(profile); }

  bool Mount(Profile* profile) override {
    // Only support mounting this volume once.
    CHECK(!mock_smbfs_);
    if (!CreateRootDirectory(profile)) {
      return false;
    }

    ash::smb_client::SmbService* smb_service =
        ash::smb_client::SmbServiceFactory::Get(profile);
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
              [&run_loop](
                  const std::vector<ash::smb_client::SmbUrl>& shares_gathered,
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
    base::RunLoop run_loop;
    smb_service->Mount(
        "SMB Share", base::FilePath("smb://server/share"), "" /* username */,
        "" /* password */, false /* use_chromad_kerberos */,
        false /* should_open_file_manager_after_mount */,
        false /* save_credentials */,
        base::BindLambdaForTesting([&](ash::smb_client::SmbMountResult result) {
          success = (result == ash::smb_client::SmbMountResult::kSuccess);
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
      const ash::smb_client::SmbFsShare::MountOptions& options,
      smbfs::SmbFsHost::Delegate* delegate) {
    std::unique_ptr<MockSmbFsMounter> mock_mounter =
        std::make_unique<MockSmbFsMounter>();
    EXPECT_CALL(*mock_mounter, Mount(_))
        .WillOnce(
            [this, delegate](smbfs::SmbFsMounter::DoneCallback mount_callback) {
              mojo::Remote<smbfs::mojom::SmbFs> smbfs_remote;
              mock_smbfs_ = std::make_unique<MockSmbFsImpl>(
                  smbfs_remote.BindNewPipeAndPassReceiver());

              std::move(mount_callback)
                  .Run(smbfs::mojom::MountError::kOk,
                       std::make_unique<smbfs::SmbFsHost>(
                           std::make_unique<ash::disks::MountPoint>(
                               mount_path(),
                               ash::disks::DiskMountManager::GetInstance()),
                           delegate, std::move(smbfs_remote),
                           delegate_.BindNewPipeAndPassReceiver()));
            });
    return std::move(mock_mounter);
  }

  std::unique_ptr<MockSmbFsImpl> mock_smbfs_;
  mojo::Remote<smbfs::mojom::SmbFsDelegate> delegate_;
};

class MockGuestOsMountProvider : public guest_os::GuestOsMountProvider {
 public:
  MockGuestOsMountProvider(Profile* profile,
                           std::string name,
                           std::string vm_type)
      : profile_(profile), name_(name) {
    if (vm_type == "bruschetta") {
      vm_type_ = guest_os::VmType::BRUSCHETTA;
    } else if (vm_type == "termina") {
      vm_type_ = guest_os::VmType::TERMINA;
    } else if (vm_type == "arcvm") {
      vm_type_ = guest_os::VmType::ARCVM;
    } else if (vm_type == "unknown") {
      vm_type_ = guest_os::VmType::UNKNOWN;
    } else {
      NOTREACHED_IN_MIGRATION();
      vm_type_ = guest_os::VmType::UNKNOWN;
    }
  }

  MockGuestOsMountProvider(const MockGuestOsMountProvider&) = delete;
  MockGuestOsMountProvider& operator=(const MockGuestOsMountProvider&) = delete;

  std::string DisplayName() override { return name_; }
  Profile* profile() override { return profile_; }
  guest_os::GuestId GuestId() override {
    return crostini::DefaultContainerId();
  }

  void Prepare(base::OnceCallback<
               void(bool success, int cid, int port, base::FilePath homedir)>
                   callback) override {
    std::move(callback).Run(true, cid_, 1234, base::FilePath());
  }

  std::unique_ptr<guest_os::GuestOsFileWatcher> CreateFileWatcher(
      base::FilePath mount_path,
      base::FilePath relative_path) override {
    return nullptr;
  }

  guest_os::VmType vm_type() override { return vm_type_; }

  int cid_;

 private:
  raw_ptr<Profile> profile_;
  std::string name_;
  guest_os::VmType vm_type_;
};

// GuestOsTestVolume: local test volume for the "Guest OS" directories.
class GuestOsTestVolume : public LocalTestVolume {
 public:
  explicit GuestOsTestVolume(Profile* profile,
                             MockGuestOsMountProvider* provider)
      : LocalTestVolume(
            util::GetGuestOsMountPointName(profile,
                                           crostini::DefaultContainerId())),
        provider_(provider) {}

  GuestOsTestVolume(const GuestOsTestVolume&) = delete;
  GuestOsTestVolume& operator=(const GuestOsTestVolume&) = delete;

  ~GuestOsTestVolume() override = default;

  bool Mount(Profile* profile) override { return CreateRootDirectory(profile); }

  const base::FilePath& mount_path() const { return root_path(); }

  raw_ptr<MockGuestOsMountProvider, DanglingUntriaged> provider_;
};

FileManagerBrowserTestBase::FileManagerBrowserTestBase() = default;

FileManagerBrowserTestBase::~FileManagerBrowserTestBase() = default;

static bool ShouldInspect(content::DevToolsAgentHost* host) {
  // TODO(crbug.com/v8/10820): Add background_page back in once
  // coverage can be collected when a background_page and app
  // share the same v8 isolate.
  if (host->GetURL().host() == ash::file_manager::kChromeUIFileManagerHost &&
      host->GetType() == "page") {
    return true;
  }

  return false;
}

bool FileManagerBrowserTestBase::ShouldForceDevToolsAgentHostCreation() {
  return !devtools_code_coverage_dir_.empty();
}

void FileManagerBrowserTestBase::DevToolsAgentHostCreated(
    content::DevToolsAgentHost* host) {
  CHECK(devtools_agent_.find(host) == devtools_agent_.end());

  if (ShouldInspect(host)) {
    devtools_agent_[host] =
        std::make_unique<coverage::DevToolsListener>(host, process_id_);
  }
}

void FileManagerBrowserTestBase::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* host) {}

void FileManagerBrowserTestBase::DevToolsAgentHostNavigated(
    content::DevToolsAgentHost* host) {
  if (devtools_agent_.find(host) == devtools_agent_.end()) {
    return;
  }

  if (ShouldInspect(host)) {
    LOG(INFO) << coverage::DevToolsListener::HostString(host, __FUNCTION__);
    devtools_agent_.find(host)->second->Navigated(host);
  } else {
    devtools_agent_.find(host)->second->Detach(host);
  }
}

void FileManagerBrowserTestBase::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* host) {}

void FileManagerBrowserTestBase::DevToolsAgentHostCrashed(
    content::DevToolsAgentHost* host,
    base::TerminationStatus status) {
  if (devtools_agent_.find(host) == devtools_agent_.end()) {
    return;
  }
  NOTREACHED_IN_MIGRATION();
}

void FileManagerBrowserTestBase::SetUp() {
  net::NetworkChangeNotifier::SetTestNotificationsOnly(true);

  extensions::MixinBasedExtensionApiTest::SetUp();
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
    // `ChromeBrowserMainPartsAsh` from adding the keepalive that normally
    // stops chromeos from shutting down unexpectedly.
    set_exit_when_last_browser_closes(false);
  }

  if (options.guest_mode == IN_GUEST_MODE) {
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchNative(ash::switches::kLoginUser, "$guest");
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kIncognito);
    set_chromeos_user_ = false;
  }

  if (options.guest_mode == IN_INCOGNITO) {
    command_line->AppendSwitch(switches::kIncognito);
  }

  if (options.offline) {
    command_line->AppendSwitchASCII(chromeos::switches::kShillStub, "clear=1");
  }

  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;

  // Make sure to run the ARC storage UI toast tests.
  enabled_features.push_back(arc::kUsbStorageUIFeature);

  if (options.enable_conflict_dialog) {
    enabled_features.push_back(ash::features::kFilesConflictDialog);
  } else {
    disabled_features.push_back(ash::features::kFilesConflictDialog);
  }

  if (options.arc) {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  if (options.single_partition_format) {
    enabled_features.push_back(ash::features::kFilesSinglePartitionFormat);
  }

  if (options.enable_drive_trash) {
    enabled_features.push_back(ash::features::kFilesTrashDrive);
  } else {
    disabled_features.push_back(ash::features::kFilesTrashDrive);
  }

  if (options.enable_dlp_files_restriction) {
    enabled_features.push_back(features::kDataLeakPreventionFilesRestriction);
  } else {
    disabled_features.push_back(features::kDataLeakPreventionFilesRestriction);
  }

  if (options.enable_files_policy_new_ux) {
    enabled_features.push_back(features::kNewFilesPolicyUX);
  } else {
    disabled_features.push_back(features::kNewFilesPolicyUX);
  }

  if (options.enable_mirrorsync) {
    enabled_features.push_back(ash::features::kDriveFsMirroring);
  } else {
    disabled_features.push_back(ash::features::kDriveFsMirroring);
  }

  if (options.enable_upload_office_to_cloud) {
    enabled_features.push_back(chromeos::features::kUploadOfficeToCloud);
  } else {
    disabled_features.push_back(chromeos::features::kUploadOfficeToCloud);
  }

  if (command_line->HasSwitch(switches::kDevtoolsCodeCoverage) &&
      options.guest_mode != IN_INCOGNITO) {
    devtools_code_coverage_dir_ =
        command_line->GetSwitchValuePath(switches::kDevtoolsCodeCoverage);
  }

  if (options.enable_arc_vm) {
    command_line->AppendSwitch(ash::switches::kEnableArcVm);
  }

  if (options.enable_file_transfer_connector) {
    enabled_features.push_back(features::kFileTransferEnterpriseConnector);
  } else {
    disabled_features.push_back(features::kFileTransferEnterpriseConnector);
  }

  if (options.enable_file_transfer_connector_new_ux) {
    enabled_features.push_back(features::kFileTransferEnterpriseConnectorUI);
  } else {
    disabled_features.push_back(features::kFileTransferEnterpriseConnectorUI);
  }

  if (options.enable_local_image_search) {
    enabled_features.push_back(ash::features::kFilesLocalImageSearch);
    enabled_features.push_back(
        ash::features::kFeatureManagementLocalImageSearch);
    enabled_features.push_back(search_features::kICASupportedByHardware);
    enabled_features.push_back(search_features::kLauncherImageSearch);
    enabled_features.push_back(search_features::kLauncherImageSearchIca);
    enabled_features.push_back(search_features::kLauncherImageSearchOcr);
  } else {
    disabled_features.push_back(ash::features::kFilesLocalImageSearch);
    disabled_features.push_back(
        ash::features::kFeatureManagementLocalImageSearch);
    disabled_features.push_back(search_features::kICASupportedByHardware);
    disabled_features.push_back(search_features::kLauncherImageSearch);
    disabled_features.push_back(search_features::kLauncherImageSearchIca);
    disabled_features.push_back(search_features::kLauncherImageSearchOcr);
  }

  if (options.enable_google_one_offer_files_banner) {
    enabled_features.push_back(ash::features::kGoogleOneOfferFilesBanner);
  } else {
    disabled_features.push_back(ash::features::kGoogleOneOfferFilesBanner);
  }

  if (options.disable_google_one_offer_files_banner) {
    enabled_features.push_back(
        ash::features::kDisableGoogleOneOfferFilesBanner);
  } else {
    disabled_features.push_back(
        ash::features::kDisableGoogleOneOfferFilesBanner);
  }

  if (options.enable_drive_bulk_pinning) {
    enabled_features.push_back(ash::features::kDriveFsBulkPinning);
    enabled_features.push_back(
        ash::features::kFeatureManagementDriveFsBulkPinning);
  } else {
    disabled_features.push_back(ash::features::kDriveFsBulkPinning);
    disabled_features.push_back(
        ash::features::kFeatureManagementDriveFsBulkPinning);
  }

  if (options.enable_cros_components) {
    enabled_features.push_back(chromeos::features::kCrosComponents);
  } else {
    disabled_features.push_back(chromeos::features::kCrosComponents);
  }

  if (options.feature_ids.size() > 0) {
    for (const std::string& feature_id : options.feature_ids) {
      base::AddTagToTestResult("feature_id", feature_id);
    }
  }

  if (options.enable_materialized_views) {
    enabled_features.push_back(ash::features::kFilesMaterializedViews);
  } else {
    disabled_features.push_back(ash::features::kFilesMaterializedViews);
  }

  if (options.enable_skyvault) {
    enabled_features.push_back(features::kSkyVault);
    enabled_features.push_back(features::kSkyVaultV2);
  } else {
    disabled_features.push_back(features::kSkyVault);
    disabled_features.push_back(features::kSkyVaultV2);
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

  extensions::MixinBasedExtensionApiTest::SetUpCommandLine(command_line);
}

bool FileManagerBrowserTestBase::SetUpUserDataDirectory() {
  if (GetOptions().guest_mode == IN_GUEST_MODE) {
    return true;
  }

  return extensions::MixinBasedExtensionApiTest::SetUpUserDataDirectory() &&
         drive::SetUpUserDataDirectoryForDriveFsTest(GetAccountId());
}

AccountId FileManagerBrowserTestBase::GetAccountId() {
  return AccountId::FromUserEmailGaiaId(
      drive::FakeDriveFsHelper::kDefaultUserEmail,
      drive::FakeDriveFsHelper::kDefaultGaiaId);
}

void FileManagerBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  extensions::MixinBasedExtensionApiTest::SetUpInProcessBrowserTestFixture();

  local_volume_ = std::make_unique<DownloadsTestVolume>();

  if (GetOptions().guest_mode == IN_GUEST_MODE) {
    return;
  }

  create_drive_integration_service_ = base::BindRepeating(
      &FileManagerBrowserTestBase::CreateDriveIntegrationService,
      base::Unretained(this));
  service_factory_for_test_ = std::make_unique<
      drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
      &create_drive_integration_service_);
}

void FileManagerBrowserTestBase::SetUpOnMainThread() {
  const Options options = GetOptions();

  // Override factory to inject a test RemoteFileSyncService.
  sync_file_system::SyncFileSystemServiceFactory::GetInstance()
      ->SetTestingFactory(
          profile(), base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
            return sync_file_system::SyncFileSystemServiceFactory::
                BuildWithRemoteFileSyncServiceForTest(
                    context,
                    std::make_unique<::testing::NiceMock<
                        sync_file_system::MockRemoteFileSyncService>>());
          }));

  extensions::MixinBasedExtensionApiTest::SetUpOnMainThread();

  CHECK(profile());
  CHECK_EQ(!!browser(), options.browser);

  if (!options.locale.empty()) {
    SwitchLanguageWaiter waiter;
    ash::locale_util::SwitchLanguage(
        options.locale, /*enable_locale_keyboard_layouts=*/true,
        /*login_layouts_only=*/false, waiter.CreateCallback(), profile());
    waiter.Wait();
  }

  if (!options.country.empty()) {
    CHECK(
        g_browser_process->variations_service()->OverrideStoredPermanentCountry(
            options.country));
  }

  if (!options.mount_volumes) {
    VolumeManager::Get(profile())->RemoveDownloadsDirectoryForTesting();
  } else {
    CHECK(local_volume_->Mount(profile()));
  }

  if (options.guest_mode != IN_GUEST_MODE) {
    // `LoggedInUserFilesAppBrowserTest` starts `embedded_test_server` via
    // `LoggedInUserMixin`. Starting the server again can cause a CHECK
    // failure.
    if (!embedded_test_server()->Started()) {
      // Start the embedded test server to serve the mocked CWS widget
      // container.
      CHECK(embedded_test_server()->Start());
    }

    drive_volume_ = drive_volumes_[profile()->GetOriginalProfile()].get();
    if (options.mount_volumes) {
      test_util::WaitUntilDriveMountPointIsAdded(profile());
    }

    // Init crostini.  Set VM and container running for testing, and register
    // CustomMountPointCallback.
    if (options.guest_mode != IN_INCOGNITO) {
      crostini_features_.set_is_allowed_now(true);
      crostini_features_.set_enabled(true);
      crostini_features_.set_root_access_allowed(true);
      crostini_features_.set_export_import_ui_allowed(true);
    }
    crostini::CrostiniManager* crostini_manager =
        crostini::CrostiniManager::GetForProfile(
            profile()->GetOriginalProfile());
    crostini_manager->set_skip_restart_for_testing();
    crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName,
                                             3);
    crostini_manager->AddRunningContainerForTesting(
        crostini::kCrostiniDefaultVmName,
        crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                                "testuser", "/home/testuser", "PLACEHOLDER_IP",
                                1234));
    crostini_volume_ = std::make_unique<CrostiniTestVolume>("sftp://3:1234");

    guest_os::GuestOsSharePathFactory::GetForProfile(
        profile()->GetOriginalProfile())
        ->RegisterGuest(crostini::DefaultContainerId());
    static_cast<ash::FakeCrosDisksClient*>(ash::CrosDisksClient::Get())
        ->AddCustomMountPointCallback(
            base::BindRepeating(&FileManagerBrowserTestBase::MaybeMountCrostini,
                                base::Unretained(this)));
    static_cast<ash::FakeCrosDisksClient*>(ash::CrosDisksClient::Get())
        ->AddCustomMountPointCallback(
            base::BindRepeating(&FileManagerBrowserTestBase::MaybeMountGuestOs,
                                base::Unretained(this)));

    if (arc::IsArcAvailable()) {
      // When ARC is available, create and register a fake FileSystemInstance
      // so ARC-related services work without a real ARC container.
      arc_file_system_instance_ =
          std::make_unique<arc::FakeFileSystemInstance>();
      arc::ArcServiceManager::Get()
          ->arc_bridge_service()
          ->file_system()
          ->SetInstance(arc_file_system_instance_.get());
      arc::WaitForInstanceReady(
          arc::ArcServiceManager::Get()->arc_bridge_service()->file_system());
      ASSERT_TRUE(arc_file_system_instance_->InitCalled());

      if (options.generic_documents_provider) {
        generic_documents_provider_volume_ =
            std::make_unique<DocumentsProviderTestVolume>(
                arc_file_system_instance_.get(), "com.example.documents",
                "root", false /* read_only */);
        if (options.mount_volumes) {
          generic_documents_provider_volume_->Mount(profile());
        }
      }
      if (options.photos_documents_provider) {
        photos_documents_provider_volume_ =
            std::make_unique<DocumentsProviderTestVolume>(
                "Google Photos", arc_file_system_instance_.get(),
                "com.google.android.apps.photos.photoprovider",
                "com.google.android.apps.photos", false /* read_only */);
        if (options.mount_volumes) {
          photos_documents_provider_volume_->Mount(profile());
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
      EXPECT_FALSE(
          file_tasks::FileTasksNotifierFactory::GetForProfile(profile()));
    }

    if (options.fake_file_system_provider) {
      file_system_provider_volume_ =
          std::make_unique<FileSystemProviderTestVolume>();
      if (options.mount_volumes) {
        file_system_provider_volume_->Mount(profile());
      }
    }
  }

  smbfs_volume_ = std::make_unique<SmbfsTestVolume>();

  hidden_volume_ = std::make_unique<HiddenTestVolume>();

  display_service_ =
      std::make_unique<NotificationDisplayServiceTester>(profile());

  process_id_ = base::GetUniqueIdForProcess().GetUnsafeValue();
  if (!devtools_code_coverage_dir_.empty()) {
    content::DevToolsAgentHost::AddObserver(this);
  }

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

  auto select_factory =
      std::make_unique<SelectFileDialogExtensionTestFactory>();
  select_factory_ = select_factory.get();
  ui::SelectFileDialog::SetFactory(std::move(select_factory));
}

void FileManagerBrowserTestBase::TearDownOnMainThread() {
  swa_web_contents_.clear();

  file_tasks_observer_.reset();
  select_factory_ = nullptr;
  ui::SelectFileDialog::SetFactory(nullptr);
  if (error_url_.is_valid()) {
    storage::CopyOrMoveOperationDelegate::SetErrorUrlForTest(nullptr);
  }
  file_manager::io_task::CopyOrMoveIOTaskImpl::SetDestinationNoSpaceForTesting(
      false);
}

void FileManagerBrowserTestBase::TearDown() {
  extensions::MixinBasedExtensionApiTest::TearDown();
  feature_list_.reset();
}

void FileManagerBrowserTestBase::StartTest() {
  ash::SystemWebAppManager::GetForTest(profile())
      ->InstallSystemAppsForTesting();
  const std::string full_test_name = GetFullTestCaseName();
  LOG(INFO) << "FileManagerBrowserTest::StartTest " << full_test_name;

#if BUILDFLAG(ENABLE_PDF)
  // TODO(crbug.com/326487542): Remove this once the tests pass for OOPIF PDF.
  if (base::FeatureList::IsEnabled(chrome_pdf::features::kPdfOopif)) {
    static const std::vector<std::string> kSkipTests = {
        "openQuickViewPdf", "openQuickViewPdfPopup"};
    if (base::Contains(kSkipTests, full_test_name)) {
      GTEST_SKIP();
    }
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  static const base::FilePath test_extension_dir = base::FilePath(
      FILE_PATH_LITERAL("ui/file_manager/integration_tests/tsc"));
  LaunchExtension(base::DIR_GEN_TEST_DATA_ROOT, test_extension_dir,
                  GetTestExtensionManifestName());
  RunTestMessageLoop();

  if (devtools_code_coverage_dir_.empty()) {
    return;
  }

  content::DevToolsAgentHost::RemoveObserver(this);
  content::RunAllTasksUntilIdle();

  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath store =
      devtools_code_coverage_dir_.AppendASCII("webui_javascript_code_coverage");
  coverage::DevToolsListener::SetupCoverageStore(store);

  for (auto& agent : devtools_agent_) {
    auto* host = agent.first;
    if (agent.second->HasCoverage(host)) {
      agent.second->GetCoverage(host, store, full_test_name);
    }
    agent.second->Detach(host);
  }

  content::DevToolsAgentHost::DetachAllClients();
  content::RunAllTasksUntilIdle();
}

void FileManagerBrowserTestBase::LaunchExtension(base::BasePathKey root,
                                                 const base::FilePath& path,
                                                 const char* manifest_name) {
  base::FilePath root_dir;
  CHECK(base::PathService::Get(root, &root_dir));

  const base::FilePath source_path = root_dir.Append(path);
  const extensions::Extension* const extension_launched =
      LoadExtensionAsComponentWithManifest(source_path, manifest_name);
  CHECK(extension_launched)
      << "Launching: " << source_path << "/" << manifest_name;
}

void FileManagerBrowserTestBase::RunTestMessageLoop() {
  FileManagerTestMessageListener listener;

  while (true) {
    auto message = listener.GetNextMessage();

    if (message.completion ==
        FileManagerTestMessageListener::Message::Completion::kPass) {
      return;  // Test PASSED.
    }
    if (message.completion ==
        FileManagerTestMessageListener::Message::Completion::kFail) {
      ADD_FAILURE() << message.message;
      return;  // Test FAILED.
    }

    // If the message in JSON format has no command, ignore it
    // but note a reply is required: use std::string().
    std::optional<base::Value> json = base::JSONReader::Read(message.message);
    if (!json) {
      message.function->Reply(std::string());
      continue;
    }

    base::Value::Dict* dictionary = json->GetIfDict();
    const std::string* command = nullptr;
    if (!dictionary || !(command = dictionary->FindString("name"))) {
      message.function->Reply(std::string());
      continue;
    }

    // Process the command, reply with the result.
    std::string result;
    OnCommand(*command, *dictionary, &result);
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
                                           const base::Value::Dict& value,
                                           std::string* output)
    NO_THREAD_SAFETY_ANALYSIS {
  const Options options = GetOptions();

  base::ScopedAllowBlockingForTesting allow_blocking;

  if (name == "updateModificationDate") {
    // Update a local file modification date.
    const std::string* relative_path = value.FindString("localPath");
    const std::optional<double> modification_date =
        value.FindDouble("modificationDate");
    ASSERT_TRUE(relative_path);
    ASSERT_TRUE(modification_date.has_value());
    base::FilePath full_path =
        file_manager::util::GetMyFilesFolderForProfile(profile());
    full_path = full_path.AppendASCII(*relative_path);
    if (!base::PathExists(full_path)) {
      *output = "false";
      return;
    }
    base::Time modification_time =
        base::Time::FromMillisecondsSinceUnixEpoch(modification_date.value());
    if (!base::TouchFile(full_path, modification_time, modification_time)) {
      *output = "false";
      return;
    }
    *output = "true";
    return;
  }

  if (name == "isInGuestMode") {
    // Obtain if the test runs in guest or incognito mode.
    LOG(INFO) << GetTestCaseName() << " is in " << options.guest_mode
              << " mode";
    *output = options.guest_mode == NOT_IN_GUEST_MODE ? "false" : "true";

    return;
  }

  if (name == "showItemInFolder") {
    const std::string* relative_path = value.FindString("localPath");
    ASSERT_TRUE(relative_path);
    base::FilePath full_path =
        file_manager::util::GetMyFilesFolderForProfile(profile());
    full_path = full_path.AppendASCII(*relative_path);

    platform_util::ShowItemInFolder(profile(), full_path);
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

  if (name == "launchFileManager") {
    const std::string* launch_dir = value.FindString("launchDir");
    base::Value::Dict arg_value;
    if (launch_dir) {
      arg_value.Set("currentDirectoryURL", *launch_dir);
    }

    const std::string* type = value.FindString("type");
    if (type) {
      arg_value.Set("type", *type);
    }

    const base::Value::List* volume_filter = value.FindList("volumeFilter");
    if (volume_filter) {
      base::Value::List cloned_volume_filter = volume_filter->Clone();
      arg_value.Set("volumeFilter", std::move(cloned_volume_filter));
    }

    const std::string* query = value.FindString("searchQuery");
    if (query) {
      arg_value.Set("searchQuery", *query);
    }

    std::string search;
    if (launch_dir || type || volume_filter || query) {
      std::string json_args;
      base::JSONWriter::Write(arg_value, &json_args);
      search = base::StrCat(
          {"?", base::EscapeUrlEncodedData(json_args, /*use_plus=*/false)});
    }

    std::string baseURL = ash::file_manager::kChromeUIFileManagerURL;
    GURL fileAppURL(base::StrCat({baseURL, search}));
    ash::SystemAppLaunchParams params;
    params.url = fileAppURL;
    params.launch_source = apps::LaunchSource::kFromTest;

    WebContentCapturingObserver observer(fileAppURL);
    observer.StartWatchingNewWebContents();
    ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::FILE_MANAGER,
                                 params);
    observer.Wait();
    ASSERT_TRUE(observer.last_navigation_succeeded());

    const std::string app_id = GetSwaAppId(observer.web_contents());
    swa_web_contents_.insert({app_id, observer.web_contents()});
    *output = app_id;
    return;
  }

  if (name == "findSwaWindow") {
    // Only search for unknown windows.
    content::WebContents* web_contents = GetLastOpenWindowWebContents();
    if (web_contents) {
      const std::string app_id = GetSwaAppId(web_contents);
      swa_web_contents_.insert({app_id, web_contents});
      *output = app_id;
    } else {
      *output = "none";
    }
    return;
  }

  if (name == "getLastActiveTabURL") {
    BrowserList* browser_list = BrowserList::GetInstance();
    Browser* browser = browser_list->GetLastActive();
    if (!browser) {
      return;
    }
    content::WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    *output = active_web_contents->GetVisibleURL().spec();
    return;
  }

  if (name == "expectWindowOrigin") {
    const std::string* expected_origin = value.FindString("expectedOrigin");
    EXPECT_TRUE(expected_origin);
    for (auto* web_contents : GetAllWebContents()) {
      const std::string& origin =
          url::Origin::Create(web_contents->GetVisibleURL()).Serialize();
      if (origin == *expected_origin) {
        *output = "true";
        return;
      }
    }
    *output = "false";
    return;
  }

  if (name == "callSwaTestMessageListener") {
    // Handles equivallent of remoteCall.callRemoteTestUtil for Files.app. By
    // default Files SWA does not allow extenrnal callers to connect to it and
    // send it messages via chrome.runtime.sendMessage. Rather than allowing
    // this, which would potentially create a security vulnerability, we
    // short-circuit sending messages by directly invoking dedicated function in
    // Files SWA.
    const std::string* data = value.FindString("data");
    ASSERT_TRUE(data);
    const std::string* app_id = value.FindString("appId");

    content::WebContents* web_contents;
    if (app_id && !app_id->empty()) {
      CHECK(base::Contains(swa_web_contents_, *app_id))
          << "Couldn't find the SWA WebContents for appId: " << *app_id
          << " command data: " << *data;
      web_contents = swa_web_contents_[*app_id];
    } else {
      // Commands for the background page might send to a WebContents which is
      // in swa_web_contents_.
      web_contents = GetLastOpenWindowWebContents();
      if (!web_contents && swa_web_contents_.size() > 0) {
        // If can't find any unknown WebContents, try the last known:
        web_contents = std::prev(swa_web_contents_.end())->second;
      }
      CHECK(web_contents) << "Couldn't find the SWA WebContents without appId"
                          << " command data: " << *data;
    }
    *output = content::EvalJs(
                  web_contents,
                  base::StrCat({"test.swaTestMessageListener(", *data, ")"}))
                  .ExtractString();
    return;
  }

  if (name == "getWindows") {
    base::Value::Dict dictionary;

    int counter = 0;
    for (auto* web_contents : GetAllWebContents()) {
      const std::string& url = web_contents->GetVisibleURL().spec();
      if (base::StartsWith(url, ash::file_manager::kChromeUIFileManagerURL)) {
        std::string app_id;
        bool found = false;

        for (const auto& pair : swa_web_contents_) {
          if (pair.second == web_contents) {
            app_id = pair.first;
            dictionary.SetByDottedPath(app_id, app_id);
            found = true;
            break;
          }
        }

        if (!found) {
          app_id =
              base::StrCat({"unknow-id-", base::NumberToString(counter++)});
          dictionary.SetByDottedPath(app_id, app_id);
        }
      }
    }

    base::JSONWriter::Write(dictionary, output);
    return;
  }

  if (name == "executeScriptInChromeUntrusted") {
    for (auto* web_contents : GetAllWebContents()) {
      bool found = false;
      web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHostWithAction(
          [&value, output, &found](content::RenderFrameHost* frame) {
            const url::Origin origin = frame->GetLastCommittedOrigin();
            if (origin.GetURL() ==
                ash::file_manager::kChromeUIFileManagerUntrustedURL) {
              const std::string* script = value.FindString("data");
              EXPECT_TRUE(script);

              content::DOMMessageQueue message_queue;
              EXPECT_TRUE(content::ExecJs(frame, *script));

              std::string json;
              EXPECT_TRUE(message_queue.WaitForMessage(&json));

              base::Value result =
                  base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS)
                      .value();

              EXPECT_TRUE(result.is_string());
              *output = result.GetString();
              found = true;
              return content::RenderFrameHost::FrameIterationAction::kStop;
            }
            return content::RenderFrameHost::FrameIterationAction::kContinue;
          });
      if (found) {
        return;
      }
    }
    // Fail the test if the chrome-untrusted:// frame wasn't found.
    NOTREACHED_IN_MIGRATION();
    return;
  }

  if (name == "isDevtoolsCoverageActive") {
    bool devtools_coverage_active = !devtools_code_coverage_dir_.empty();
    LOG(INFO) << "isDevtoolsCoverageActive: " << devtools_coverage_active;
    *output = devtools_coverage_active ? "true" : "false";
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

  if (name == "getRootPaths") {
    // Obtain the root paths.
    const auto downloads_root =
        util::GetDownloadsMountPointName(profile()) + "/Downloads";

    base::Value::Dict dictionary;
    dictionary.Set("downloads", "/" + downloads_root);

    base::FilePath my_files =
        file_manager::util::GetMyFilesFolderForProfile(profile());
    dictionary.Set("my_files", my_files.MaybeAsASCII());

    if (!profile()->IsGuestSession()) {
      auto* drive_integration_service =
          drive::DriveIntegrationServiceFactory::GetForProfile(profile());
      if (drive_integration_service->IsMounted()) {
        const auto drive_mount_name =
            drive_integration_service->GetMountPointPath().BaseName();
        dictionary.Set("drive",
                       base::StrCat({"/", drive_mount_name.value(), "/root"}));
      }
      if (android_files_volume_) {
        dictionary.Set("android_files",
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
    std::string origin = url.DeprecatedGetOriginAsURL().spec();
    if (*origin.rbegin() == '/') {  // Strip origin trailing '/'.
      origin.resize(origin.length() - 1);
    }

    base::Value::Dict dictionary;
    dictionary.Set("url", url.spec());
    dictionary.Set("origin", origin);

    base::JSONWriter::Write(dictionary, output);
    return;
  }

  if (name == "addEntries") {
    // Add the message.entries to the message.volume.
    AddEntriesMessage message;
    ASSERT_TRUE(AddEntriesMessage::ConvertJSONValue(value, &message))
        << value.DebugString();

    for (size_t i = 0; i < message.entries.size(); ++i) {
      switch (message.volume) {
        case AddEntriesMessage::LOCAL_VOLUME:
          local_volume_->CreateEntry(*message.entries[i]);
          break;
        case AddEntriesMessage::MY_FILES:
          local_volume_->CreateEntryAtRoot(*message.entries[i]);
          break;
        case AddEntriesMessage::CROSTINI_VOLUME:
          CHECK(crostini_volume_);
          ASSERT_TRUE(crostini_volume_->Initialize(profile()));
          crostini_volume_->CreateEntry(*message.entries[i]);
          break;
        case AddEntriesMessage::GUEST_OS_VOLUME_0:
          CHECK(guest_os_volumes_.size() > 0)
              << "Must call registerMountableGuest first";
          guest_os_volumes_["sftp://0:1234"]->CreateEntry(*message.entries[i]);
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
        case AddEntriesMessage::GENERIC_DOCUMENTS_PROVIDER_VOLUME:
          if (generic_documents_provider_volume_) {
            generic_documents_provider_volume_->CreateEntry(
                *message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no DocumentsProvider volume.";
          }
          break;
        case AddEntriesMessage::PHOTOS_DOCUMENTS_PROVIDER_VOLUME:
          if (photos_documents_provider_volume_) {
            photos_documents_provider_volume_->CreateEntry(*message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no Photos DocumentsProvider volume.";
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
        case AddEntriesMessage::MEDIA_VIEW_DOCUMENTS:
          if (media_view_documents_) {
            media_view_documents_->CreateEntry(*message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no MediaView Documents volume.";
          }
          break;
        case AddEntriesMessage::PROVIDED_VOLUME:
          if (file_system_provider_volume_) {
            file_system_provider_volume_->CreateEntry(profile(),
                                                      *message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no fileSystemProvider volume.";
          }
          break;
        case AddEntriesMessage::SMBFS_VOLUME:
          CHECK(smbfs_volume_);
          ASSERT_TRUE(smbfs_volume_->Initialize(profile()));
          smbfs_volume_->CreateEntry(*message.entries[i]);
          break;
        case AddEntriesMessage::MTP_VOLUME:
          if (mtp_volume_) {
            mtp_volume_->CreateEntry(*message.entries[i]);
          } else {
            LOG(FATAL) << "Add entry: but no MTP volume.";
          }
          break;
      }
    }

    return;
  }

  if (name == "mountFakeUsb" || name == "mountFakeUsbEmpty" ||
      name == "mountFakeUsbDcim") {
    std::string file_system = "ext4";
    const std::string* file_system_param = value.FindString("filesystem");
    if (file_system_param) {
      file_system = *file_system_param;
    }
    usb_volume_ = std::make_unique<RemovableTestVolume>(
        "fake-usb", VOLUME_TYPE_REMOVABLE_DISK_PARTITION, ash::DeviceType::kUSB,
        base::FilePath(), "FAKEUSB", file_system);

    if (name == "mountFakeUsb") {
      ASSERT_TRUE(usb_volume_->PrepareTestEntries(profile()));
    } else if (name == "mountFakeUsbDcim") {
      ASSERT_TRUE(usb_volume_->PrepareDcimTestEntries(profile()));
    }

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
        ash::DeviceType::kUSB, device_path, "Drive Label", "ext4");
    partition_2_ = std::make_unique<RemovableTestVolume>(
        "partition-2", VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
        ash::DeviceType::kUSB, device_path, "Drive Label", "ext4");

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
        ash::DeviceType::kUSB, device_path, "Drive Label", "ntfs");
    partition_2_ = std::make_unique<RemovableTestVolume>(
        "partition-2", VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
        ash::DeviceType::kUSB, device_path, "Drive Label", "ext4");
    partition_3_ = std::make_unique<RemovableTestVolume>(
        "partition-3", VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
        ash::DeviceType::kUSB, device_path, "Drive Label", "vfat");

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
    mtp_volume_ = std::make_unique<FakeTestVolume>("fake-mtp", VOLUME_TYPE_MTP,
                                                   ash::DeviceType::kUnknown);

    if (name == "mountFakeMtp") {
      ASSERT_TRUE(mtp_volume_->PrepareTestEntries(profile()));
    }

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
        "com.android.providers.media.documents", arc::kImagesRootId);
    media_view_videos_ = std::make_unique<MediaViewTestVolume>(
        arc_file_system_instance_.get(),
        "com.android.providers.media.documents", arc::kVideosRootId);
    media_view_audio_ = std::make_unique<MediaViewTestVolume>(
        arc_file_system_instance_.get(),
        "com.android.providers.media.documents", arc::kAudioRootId);
    media_view_documents_ = std::make_unique<MediaViewTestVolume>(
        arc_file_system_instance_.get(),
        "com.android.providers.media.documents", arc::kDocumentsRootId);

    ASSERT_TRUE(media_view_images_->Mount(profile()));
    ASSERT_TRUE(media_view_videos_->Mount(profile()));
    ASSERT_TRUE(media_view_audio_->Mount(profile()));
    ASSERT_TRUE(media_view_documents_->Mount(profile()));
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

  if (name == "mountHidden") {
    DCHECK(hidden_volume_);
    ASSERT_TRUE(hidden_volume_->Mount(profile()));
    return;
  }

  if (name == "setOfficeFileHandler") {
    file_manager::file_tasks::SetWordFileHandlerToFilesSWA(
        profile(), file_manager::file_tasks::kActionIdWebDriveOfficeWord);
    return;
  }

  if (name == "setDriveEnabled") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    profile()->GetPrefs()->SetBoolean(drive::prefs::kDisableDrive,
                                      !enabled.value());
    return;
  }

  if (name == "setLocalFilesEnabled") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    g_browser_process->local_state()->SetBoolean(prefs::kLocalUserFilesAllowed,
                                                 enabled.value());
    return;
  }

  if (name == "setLocalFilesMigrationDestination") {
    const std::string* provider = value.FindString("provider");
    ASSERT_TRUE(provider);
    ASSERT_TRUE(*provider == download_dir_util::kLocationGoogleDrive ||
                *provider == download_dir_util::kLocationOneDrive);
    g_browser_process->local_state()->SetString(
        prefs::kLocalUserFilesMigrationDestination, *provider);
    return;
  }

  if (name == "skipSkyVaultMigration") {
    file_manager::VolumeManager* volume_manager = VolumeManager::Get(profile());
    volume_manager->OnMigrationSucceededForTesting();
    return;
  }

  if (name == "setDefaultLocation") {
    const std::string* defaultLocation = value.FindString("defaultLocation");
    ASSERT_TRUE(defaultLocation &&
                (*defaultLocation == download_dir_util::kLocationGoogleDrive ||
                 *defaultLocation == download_dir_util::kLocationOneDrive));
    profile()->GetPrefs()->SetString(prefs::kFilesAppDefaultLocation,
                                     *defaultLocation);
    return;
  }

  if (name == "setTrashEnabled") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    profile()->GetPrefs()->SetBoolean(ash::prefs::kFilesAppTrashEnabled,
                                      enabled.value());
    return;
  }

  if (name == "setPdfPreviewEnabled") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysOpenPdfExternally,
                                      !enabled.value());
    return;
  }

  if (name == "setPrefOfficeFileMovedToGoogleDrive") {
    std::optional<int64_t> timestamp = value.FindDouble("timestamp");
    ASSERT_TRUE(timestamp.has_value());
    profile()->GetPrefs()->SetTime(
        prefs::kOfficeFileMovedToGoogleDrive,
        base::Time::FromMillisecondsSinceUnixEpoch(timestamp.value()));
    return;
  }

  if (name == "setSpacedFreeSpace") {
    const std::string* space = value.FindString("freeSpace");
    ASSERT_TRUE(space) << "No freeSpace supplied";
    int64_t free_space;
    ASSERT_TRUE(base::StringToInt64(*space, &free_space))
        << "Couldn't convert string to int64";
    ash::FakeSpacedClient::Get()->set_free_disk_space(free_space);
    return;
  }

  if (name == "forcePinningManagerSpaceCheck") {
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    ASSERT_NE(integration_service, nullptr);
    ASSERT_NE(integration_service->GetPinningManager(), nullptr);
    integration_service->GetPinningManager()->CheckFreeSpace();
    return;
  }

  if (name == "setBulkPinningEnabledPref") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    profile()->GetPrefs()->SetBoolean(drive::prefs::kDriveFsBulkPinningEnabled,
                                      enabled.value());
    return;
  }

  if (name == "setBulkPinningOnline") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    ASSERT_NE(integration_service, nullptr);
    ASSERT_NE(integration_service->GetPinningManager(), nullptr);
    integration_service->GetPinningManager()->SetOnline(enabled.value());
    return;
  }

  if (name == "forceBulkPinningCalculateRequiredSpace") {
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    ASSERT_NE(integration_service, nullptr);
    ASSERT_NE(integration_service->GetPinningManager(), nullptr);
    ASSERT_TRUE(
        integration_service->GetPinningManager()->CalculateRequiredSpace());
    return;
  }

  if (name == "getBulkPinningStage") {
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    ASSERT_NE(integration_service, nullptr);
    ASSERT_NE(integration_service->GetPinningManager(), nullptr);
    auto progress = integration_service->GetPinningManager()->GetProgress();
    *output = drivefs::pinning::ToString(progress.stage);
    return;
  }

  if (name == "getBulkPinningRequiredSpace") {
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    ASSERT_NE(integration_service, nullptr);
    ASSERT_NE(integration_service->GetPinningManager(), nullptr);
    auto progress = integration_service->GetPinningManager()->GetProgress();
    *output = base::NumberToString(progress.required_space);
    return;
  }

  if (name == "setBulkPinningShouldPinFiles") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value())
        << "enabled must be sent with setBulkPiningDontPinFiles";
    auto* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    ASSERT_NE(integration_service, nullptr);
    ASSERT_NE(integration_service->GetPinningManager(), nullptr);
    integration_service->GetPinningManager()->SetShouldPinFilesForTesting(
        enabled.value());
    return;
  }

  if (name == "setCrostiniEnabled") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    profile()->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled,
                                      enabled.value());
    if (enabled.value()) {
      guest_os::GuestOsSharePathFactory::GetForProfile(profile())
          ->RegisterGuest(crostini::DefaultContainerId());
    } else {
      guest_os::GuestOsSharePathFactory::GetForProfile(profile())
          ->UnregisterGuest(crostini::DefaultContainerId());
    }
    return;
  }

  if (name == "setCrostiniRootAccessAllowed") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    crostini_features_.set_root_access_allowed(enabled.value());
    return;
  }

  if (name == "setCrostiniExportImportAllowed") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    crostini_features_.set_export_import_ui_allowed(enabled.value());
    return;
  }

  if (name == "useCellularNetwork") {
    net::NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChangeForTests(
        net::NetworkChangeNotifier::GetMaxBandwidthMbpsForConnectionSubtype(
            net::NetworkChangeNotifier::SUBTYPE_HSPA),
        net::NetworkChangeNotifier::CONNECTION_3G);
    return;
  }

  if (name == "setDriveConnectionStatus") {
    using drive::util::ConnectionStatus;
    using drive::util::SetDriveConnectionStatusForTesting;

    const std::string* status = value.FindString("status");
    ASSERT_TRUE(status) << "Require status to update drive connection state";

    if (*status == "no_service") {
      SetDriveConnectionStatusForTesting(ConnectionStatus::kNoService);
    } else if (*status == "no_network") {
      SetDriveConnectionStatusForTesting(ConnectionStatus::kNoNetwork);
    } else if (*status == "not_ready") {
      SetDriveConnectionStatusForTesting(ConnectionStatus::kNotReady);
    } else if (*status == "metered") {
      SetDriveConnectionStatusForTesting(ConnectionStatus::kMetered);
    } else if (*status == "connected") {
      SetDriveConnectionStatusForTesting(ConnectionStatus::kConnected);
    } else {
      NOTREACHED_IN_MIGRATION()
          << "Unknown status (" << *status << ") provided";
    }

    auto* const service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile());
    ASSERT_NE(service, nullptr);
    service->OnNetworkChanged();
    return;
  }

  if (name == "setSyncOnMeteredNetwork") {
    std::optional<bool> enabled = value.FindBool("enabled");
    ASSERT_TRUE(enabled.has_value());
    profile()->GetPrefs()->SetBoolean(drive::prefs::kDisableDriveOverCellular,
                                      !enabled.value());
    return;
  }

  if (name == "clickNotificationButton") {
    const std::string* extension_id = value.FindString("extensionId");
    ASSERT_TRUE(extension_id);
    const std::string* notification_id = value.FindString("notificationId");
    ASSERT_TRUE(notification_id);

    const std::string delegate_id = *extension_id + "-" + *notification_id;
    std::optional<message_center::Notification> notification =
        display_service_->GetNotification(delegate_id);
    EXPECT_TRUE(notification);

    std::optional<int> index = value.FindInt("index");
    ASSERT_TRUE(index);
    display_service_->SimulateClick(NotificationHandler::Type::EXTENSION,
                                    delegate_id, *index, std::nullopt);
    return;
  }

  if (name == "launchProviderExtension") {
    const std::string* manifest = value.FindString("manifest");
    ASSERT_TRUE(manifest);
    LaunchExtension(base::DIR_SRC_TEST_DATA_ROOT,
                    base::FilePath(FILE_PATH_LITERAL(
                        "ui/file_manager/integration_tests/testing_provider")),
                    (*manifest).c_str());
    return;
  }

  if (name == "dispatchNativeMediaKey") {
    ui::KeyEvent key_event(ui::EventType::kKeyPressed,
                           ui::VKEY_MEDIA_PLAY_PAUSE, 0);
    ASSERT_TRUE(PostKeyEvent(&key_event));
    *output = "mediaKeyDispatched";
    return;
  }

  if (name == "dispatchTabKey") {
    // Read optional modifier parameter |shift|.
    bool shift = value.FindBool("shift").value_or(false);

    int flag = shift ? ui::EF_SHIFT_DOWN : 0;
    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_TAB, flag);
    ASSERT_TRUE(PostKeyEvent(&key_event));
    *output = "tabKeyDispatched";
    return;
  }

  if (name == "simulateClick") {
    std::optional<int> click_x = value.FindInt("clickX");
    std::optional<int> click_y = value.FindInt("clickY");
    ASSERT_TRUE(click_x);
    ASSERT_TRUE(click_y);
    const std::string* app_id = value.FindString("appId");
    ASSERT_TRUE(app_id);

    content::WebContents* web_contents;
    CHECK(base::Contains(swa_web_contents_, *app_id))
        << "Couldn't find the SWA WebContents for appId: " << *app_id;
    web_contents = swa_web_contents_[*app_id];

    std::optional<bool> leftClick = value.FindBool("leftClick");
    ASSERT_TRUE(leftClick.has_value());
    auto button = leftClick.value() ? blink::WebMouseEvent::Button::kLeft
                                    : blink::WebMouseEvent::Button::kRight;
    SimulateMouseClickAt(web_contents, 0 /* modifiers */, button,
                         gfx::Point(*click_x, *click_y));
    return;
  }

  if (name == "hasSwaStarted") {
    const std::string* swa_app_id = value.FindString("swaAppId");
    ASSERT_TRUE(swa_app_id);

    *output = "false";

    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->InstanceRegistry().ForEachInstance(
        [swa_app_id, &output](const apps::InstanceUpdate& update) {
          if (update.AppId() == *swa_app_id &&
              update.State() & apps::InstanceState::kStarted) {
            *output = "true";
          }
        });

    return;
  }

  if (name == "getVolumesCount") {
    file_manager::VolumeManager* volume_manager = VolumeManager::Get(profile());
    *output = base::NumberToString(base::ranges::count_if(
        volume_manager->GetVolumeList(),
        [](const auto& volume) { return !volume->hidden(); }));
    return;
  }

  if (name == "disableTabletMode") {
    ash::ShellTestApi().SetTabletModeEnabledForTest(false);
    *output = "tabletModeDisabled";
    return;
  }

  if (name == "enableTabletMode") {
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);
    *output = "tabletModeEnabled";
    return;
  }

  // Spawn the open file window, the one which is invoked by Ctrl+O. Since this
  // window typically is used to navigate the browser to the local file, it
  // stores the navigation observer, which later could be used via the
  // `waitForSelectFileDialogNavigation` message.
  if (name == "runSelectFileDialog") {
    browser()->OpenFile();
    test_navigation_observer_ =
        std::make_unique<content::TestNavigationObserver>(
            browser()->tab_strip_model()->GetActiveWebContents(), 1);
    return;
  }

  // Waits for the navigation which will happen or happened since a stored
  // navigation observer was created. Return the URL which the browser was
  // navigated to.
  if (name == "waitForSelectFileDialogNavigation") {
    if (!test_navigation_observer_) {
      *output = "";
      return;
    }
    test_navigation_observer_->Wait();
    *output = test_navigation_observer_->last_navigation_url().spec();
    test_navigation_observer_.reset();
    return;
  }

  if (name == "isConflictDialogEnabled") {
    *output = options.enable_conflict_dialog ? "true" : "false";
    return;
  }

  if (name == "isSmbEnabled") {
    *output = options.native_smb ? "true" : "false";
    return;
  }

  if (name == "isBannersFrameworkEnabled") {
    *output = options.enable_banners_framework ? "true" : "false";
    return;
  }

  if (name == "isMirrorSyncEnabled") {
    *output = options.enable_mirrorsync ? "true" : "false";
    return;
  }

  if (name == "switchLanguage") {
    const std::string* language = value.FindString("language");
    ASSERT_TRUE(language);
    base::RunLoop run_loop;
    ash::locale_util::SwitchLanguage(
        *language, true, false,
        base::BindRepeating(
            [](base::RunLoop* run_loop,
               const ash::locale_util::LanguageSwitchResult&) {
              run_loop->Quit();
            },
            &run_loop),
        profile());
    run_loop.Run();
    return;
  }

  if (name == "setTimezone") {
    const std::string* timezone = value.FindString("timezone");
    ASSERT_TRUE(timezone);
    auto* user = user_manager::UserManager::Get()->GetActiveUser();
    ash::system::SetSystemTimezone(user, *timezone);
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

  if (name == "getHistogramSum") {
    GetTotalHistogramSum message;
    ASSERT_TRUE(GetTotalHistogramSum::ConvertJSONValue(value, &message));
    // GetTotalSum returns an int64_t which does not conform to JSON, convert to
    // a string to ensure it can be JSON encoded.
    base::JSONWriter::Write(
        base::Value(base::NumberToString(
            histograms_.GetTotalSum(message.histogram_name))),
        output);
    return;
  }

  if (name == "expectHistogramTotalCount") {
    ExpectHistogramTotalCountMessage message;
    ASSERT_TRUE(
        ExpectHistogramTotalCountMessage::ConvertJSONValue(value, &message));
    histograms_.ExpectTotalCount(message.histogram_name, message.count);

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
    static_cast<ash::FakeCrosDisksClient*>(ash::CrosDisksClient::Get())
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

  if (name == "onDropFailedPluginVmDirectoryNotShared") {
    EventRouterFactory::GetForProfile(profile())
        ->DropFailedPluginVmDirectoryNotShared();
    return;
  }

  if (name == "displayEnableDocsOfflineDialog") {
    drive_volume_->DisplayConfirmDialog(drivefs::mojom::DialogReason::New(
        drivefs::mojom::DialogReason::Type::kEnableDocsOffline,
        base::FilePath()));
    return;
  }

  if (name == "setDrivePinSyncingEvent") {
    const std::string* path = value.FindString("path");
    ASSERT_TRUE(path);
    std::optional<int64_t> bytes_transferred =
        value.FindInt("bytesTransferred");
    std::optional<int64_t> bytes_to_transfer = value.FindInt("bytesToTransfer");
    ASSERT_TRUE(bytes_transferred.has_value());
    ASSERT_TRUE(bytes_to_transfer.has_value());
    using EventState = drivefs::mojom::ItemEvent::State;
    EventState state = EventState::kQueued;
    if (bytes_transferred < bytes_to_transfer) {
      state = EventState::kInProgress;
    } else if (bytes_transferred == bytes_to_transfer) {
      state = EventState::kCompleted;
    }
    drive_volume_->SetFileSyncStatus(
        path, state, drivefs::mojom::ItemEventReason::kPin,
        bytes_transferred.value(), bytes_to_transfer.value());
    return;
  }

  if (name == "setDriveSyncProgress") {
    auto* path = value.FindString("path");
    auto progress = value.FindInt("progress");
    ASSERT_TRUE(path);
    ASSERT_TRUE(progress.has_value());
    drive_volume_->SetFileProgress(path, *progress);
    return;
  }

  if (name == "setDriveSyncError") {
    auto* path = value.FindString("path");
    ASSERT_TRUE(path);
    drive_volume_->SetSyncError(path);
    return;
  }

  if (name == "getLastDriveDialogResult") {
    std::optional<drivefs::mojom::DialogResult> result =
        drive_volume_->last_dialog_result();
    base::JSONWriter::Write(
        base::Value(result ? static_cast<int32_t>(result.value()) : -1),
        output);
    return;
  }

  if (name == "isItemPinned") {
    const std::string* path = value.FindString("path");
    ASSERT_TRUE(path) << "No supplied path to isItemPinned";
    std::optional<bool> is_pinned = drive_volume_->IsItemPinned(*path);
    ASSERT_TRUE(is_pinned.has_value()) << "Supplied path is unknown: " << *path;
    base::JSONWriter::Write(base::Value(is_pinned.value()), output);
    return;
  }

  if (name == "setCanPin") {
    const std::string* path = value.FindString("path");
    ASSERT_TRUE(path) << "No supplied path to setCanPin";
    std::optional<bool> can_pin = value.FindBool("canPin");
    ASSERT_TRUE(can_pin.has_value()) << "Need to supply canPin";
    drive_volume_->SetCanPin(*path, can_pin.value());
    return;
  }

  if (name == "setPooledStorageQuotaUsage") {
    std::optional<int64_t> used_user_bytes = value.FindInt("usedUserBytes");
    ASSERT_TRUE(used_user_bytes.has_value())
        << "Need usedUserBytes to set pooled storage quota used";
    std::optional<int64_t> total_user_bytes = value.FindInt("totalUserBytes");
    ASSERT_TRUE(total_user_bytes.has_value())
        << "Need totalUserBytes to set pooled storage quota used";
    std::optional<bool> organization_limit_exceeded =
        value.FindBool("organizationLimitExceeded");
    ASSERT_TRUE(organization_limit_exceeded.has_value())
        << "Need organizationLimitExceeded to set pooled storage quota used";
    drive_volume_->SetPooledStorageQuotaUsage(
        used_user_bytes.value(), total_user_bytes.value(),
        organization_limit_exceeded.value());
    return;
  }

  if (name == "sendDriveCloudDeleteEvent") {
    const std::string* path = value.FindString("path");
    ASSERT_TRUE(path) << "No supplied path to sendDriveFilesChangedEvent";
    drive_volume_->SendCloudDeleteEvent(*path);
    return;
  }

  if (name == "isCrosComponents") {
    *output = options.enable_cros_components ? "true" : "false";
    return;
  }

  if (name == "setDeviceOffline") {
    ash::ShillServiceClient::Get()->GetTestInterface()->ClearServices();
    content::NetworkConnectionChangeSimulator().SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
    return;
  }

  if (name == "setupImageTerms") {
    const std::string* path = value.FindString("path");
    ASSERT_TRUE(path) << "Missing file path for setupImageTerms";
    const std::string* terms = value.FindString("terms");
    ASSERT_TRUE(terms) << "Missing terms for setupImageTerms";

    base::FilePath file_path = local_volume_->GetFilePath(*path);
    std::vector<std::string> tokens = base::SplitString(
        *terms, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    std::set<std::string> unique_terms(tokens.begin(), tokens.end());
    app_list::ImageInfo image_info(unique_terms, file_path, base::Time::Now(),
                                   /*file_size*/ 1);
    app_list::LocalImageSearchServiceFactory::GetForBrowserContext(profile())
        ->Insert(image_info);
    return;
  }

  if (name == "getSharesheetInfo") {
    views::Widget* sharesheet_widget = FindSharesheetWidget();
    base::Value::List result;
    if (sharesheet_widget) {
      views::View* sharesheet_bubble_view =
          sharesheet_widget->GetContentsView();
      views::View* targets = sharesheet_bubble_view->GetViewByID(
          ash::sharesheet::SharesheetViewID::TARGETS_DEFAULT_VIEW_ID);
      for (views::View* button : targets->children()) {
        views::Label* label = static_cast<views::Label*>(button->GetViewByID(
            ash::sharesheet::SharesheetViewID::TARGET_LABEL_VIEW_ID));
        result.Append(label->GetText());
      }
    }
    base::JSONWriter::Write(result, output);
    return;
  }

  if (name == "focusWindow") {
    const std::string* app_id = value.FindString("appId");
    ASSERT_TRUE(app_id);

    content::WebContents* web_contents;
    CHECK(base::Contains(swa_web_contents_, *app_id))
        << "Couldn't find the SWA WebContents for appId: " << *app_id;
    web_contents = swa_web_contents_[*app_id];
    web_contents->Focus();
    return;
  }

  if (name == "mockDriveReadFailure") {
    const std::string path = "v2/root/" + *value.FindString("path");
    base::FilePath user_data_directory;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
    error_url_ = storage::FileSystemURL::CreateForTest(
        blink::StorageKey::CreateFirstParty(url::Origin::Create(
            GURL("chrome://file-manager/external/" + path))),
        /*mount_type*/ storage::kFileSystemTypeExternal,
        /*virtual_path*/ base::FilePath(path),
        /*mount_filesystem_id*/ {},
        /*cracked_type*/ storage::kFileSystemTypeDriveFs,
        /*cracked_path*/
        user_data_directory.Append(base::FilePath("user/drive/" + path)),
        /*filesystem_id*/ "v2",
        /*mount_option*/ {});
    storage::CopyOrMoveOperationDelegate::SetErrorUrlForTest(&error_url_);
    return;
  }

  if (name == "mockIOTaskDestinationNoSpace") {
    file_manager::io_task::CopyOrMoveIOTaskImpl::
        SetDestinationNoSpaceForTesting(true);
    return;
  }

  if (HandleGuestOsCommands(name, value, output)) {
    return;
  }

  if (HandleDlpCommands(name, value, output)) {
    return;
  }

  if (HandleEnterpriseConnectorCommands(name, value, output)) {
    return;
  }

  if (HandleSkyVaultCommands(name, value, output)) {
    return;
  }

  FAIL() << "Unknown test message: " << name;
}  // NOLINT(readability/fn_size): Structure of OnCommand function should be
   // easy to manage.

bool FileManagerBrowserTestBase::HandleGuestOsCommands(
    const std::string& name,
    const base::Value::Dict& value,
    std::string* output) {
  if (name == "registerMountableGuest") {
    const std::string* displayName = value.FindString("displayName");
    const base::Value* canMount = value.Find("canMount");
    const std::string* vmType = value.FindString("vmType");
    CHECK(displayName != nullptr);
    // TODO(davidmunro): Merge with in-constructor derivation.
    // auto id = guest_os::GuestId(guest_os::VmType::UNKNOWN, *displayName,
    // *displayName);
    auto* registry = guest_os::GuestOsServiceFactory::GetForProfile(profile())
                         ->MountProviderRegistry();
    auto id = registry->Register(std::make_unique<MockGuestOsMountProvider>(
        profile()->GetOriginalProfile(), *displayName,
        vmType ? *vmType : "bruschetta"));
    MockGuestOsMountProvider* ptr =
        reinterpret_cast<MockGuestOsMountProvider*>(registry->Get(id));
    ptr->cid_ = id;
    if (canMount && canMount->GetBool()) {
      // If we ask for the volume to be mountable we add it to the map, and it's
      // mountable. If not then it's an unknown volume and the mount request
      // fails.
      guest_os_volumes_[base::StringPrintf("sftp://%d:1234", id)] =
          std::make_unique<GuestOsTestVolume>(profile(), ptr);
    }

    base::JSONWriter::Write(base::Value(id), output);
    return true;
  }
  if (name == "unregisterMountableGuest") {
    int id;
    const std::string* str = value.FindString("guestId");
    CHECK(str != nullptr);
    CHECK(base::StringToInt(*str, &id));
    auto* registry = guest_os::GuestOsServiceFactory::GetForProfile(profile())
                         ->MountProviderRegistry();
    registry->Unregister(id);
    return true;
  }
  if (name == "unmountGuest") {
    int id;
    const std::string* str = value.FindString("guestId");
    CHECK(str != nullptr);
    CHECK(base::StringToInt(*str, &id));
    auto* registry = guest_os::GuestOsServiceFactory::GetForProfile(profile())
                         ->MountProviderRegistry();
    registry->Get(id)->Unmount();
    return true;
  }
  return false;
}

bool FileManagerBrowserTestBase::HandleDlpCommands(
    const std::string& name,
    const base::Value::Dict& value,
    std::string* output) {
  // DLP commands are only handled by the DlpFilesAppBrowserTest.
  return false;
}

bool FileManagerBrowserTestBase::HandleEnterpriseConnectorCommands(
    const std::string& name,
    const base::Value::Dict& value,
    std::string* output) {
  // Enterprise connector commands are only handled by the
  // FileTransferConnectorFilesAppBrowserTest.
  return false;
}

bool FileManagerBrowserTestBase::HandleSkyVaultCommands(
    const std::string& name,
    const base::Value::Dict& value,
    std::string* output) {
  // SkyVault commands are only handled by the
  // SkyVaultFilesAppBrowserTest.
  return false;
}

drive::DriveIntegrationService*
FileManagerBrowserTestBase::CreateDriveIntegrationService(Profile* profile) {
  const Options options = GetOptions();
  drive_volumes_[profile->GetOriginalProfile()] =
      std::make_unique<DriveFsTestVolume>(profile->GetOriginalProfile());
  if (options.guest_mode != IN_INCOGNITO && options.mount_volumes &&
      profile->GetBaseName().value() == "user") {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  if (source_url.scheme() != "sftp") {
    return {};
  }
  if (source_path != crostini_volume_->source_path()) {
    return {};
  }
  CHECK(crostini_volume_->Mount(profile()));
  return crostini_volume_->mount_path();
}

base::FilePath FileManagerBrowserTestBase::MaybeMountGuestOs(
    const std::string& source_path,
    const std::vector<std::string>& mount_options) {
  GURL source_url(source_path);
  DCHECK(source_url.is_valid());
  if (source_url.scheme() != "sftp") {
    return {};
  }
  if (!guest_os_volumes_.contains(source_path)) {
    return {};
  }
  guest_os_volumes_[source_path]->Mount(profile());
  return guest_os_volumes_[source_path]->mount_path();
}

void FileManagerBrowserTestBase::EnableVirtualKeyboard() {
  ash::ShellTestApi().EnableVirtualKeyboard();
}
std::string FileManagerBrowserTestBase::GetSwaAppId(
    content::WebContents* web_contents) {
  CHECK(web_contents);

  return content::EvalJs(web_contents, "test.getSwaAppId()").ExtractString();
}

std::vector<content::WebContents*>
FileManagerBrowserTestBase::GetAllWebContents() {
  return content::GetAllWebContents();
}

content::WebContents* FileManagerBrowserTestBase::GetWebContentsForId(
    const std::string& app_id) {
  return swa_web_contents_.at(app_id);
}

content::WebContents*
FileManagerBrowserTestBase::GetLastOpenWindowWebContents() {
  for (auto* web_contents : GetAllWebContents()) {
    const std::string& url = web_contents->GetVisibleURL().spec();
    if (base::StartsWith(url, ash::file_manager::kChromeUIFileManagerURL) &&
        !web_contents->IsLoading()) {
      if (swa_web_contents_.size() == 0) {
        return web_contents;
      }

      // Ignore known WebContents.
      if (!base::Contains(swa_web_contents_, web_contents,
                          &IdToWebContents::value_type::second)) {
        return web_contents;
      }
    }
  }

  LOG(WARNING) << "Failed to retrieve WebContents in swa mode";
  return nullptr;
}

bool FileManagerBrowserTestBase::PostKeyEvent(ui::KeyEvent* key_event) {
  gfx::NativeWindow native_window = gfx::NativeWindow();

  content::WebContents* web_contents = GetLastOpenWindowWebContents();
  if (!web_contents && swa_web_contents_.size() > 0) {
    // If can't find any unknown WebContents, try the last known:
    web_contents = std::prev(swa_web_contents_.end())->second;
  }
  if (web_contents) {
    const Browser* browser = chrome::FindBrowserWithTab(web_contents);
    if (browser) {
      BrowserWindow* window = browser->window();
      if (window) {
        native_window = window->GetNativeWindow();
      }
    }
  }
  if (!native_window) {
    // Try to get the save as/open with dialog.
    if (select_factory_) {
      content::RenderFrameHost* frame_host = select_factory_->GetFrameHost();
      if (frame_host) {
        native_window = frame_host->GetNativeView()->GetToplevelWindow();
      }
    }
  }
  if (native_window) {
    native_window->GetHost()->DispatchKeyEventPostIME(key_event);
    return true;
  }
  return false;
}

}  // namespace file_manager
