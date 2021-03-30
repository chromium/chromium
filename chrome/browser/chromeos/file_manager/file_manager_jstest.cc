// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"
#include "content/public/test/browser_test.h"

class FileManagerJsTest : public FileManagerJsTestBase {
 protected:
  FileManagerJsTest() : FileManagerJsTestBase(
      base::FilePath(FILE_PATH_LITERAL("ui/file_manager/file_manager"))) {}
};

// Tests that draw to canvases and test pixels need pixel output turned on.
class CanvasFileManagerJsTest : public FileManagerJsTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    FileManagerJsTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ActionsModelTest) {
  RunTestURL("foreground/js/actions_model_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ActionsSubmenuTest) {
  RunTestURL("foreground/js/ui/actions_submenu_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Breadcrumb) {
  RunTestURL("foreground/js/ui/breadcrumb_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ContentMetadataProvider) {
  RunTestURL(
      "foreground/js/metadata/content_metadata_provider_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Crostini) {
  RunTestURL("background/js/crostini_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DeviceHandlerTest) {
  RunTestURL("background/js/device_handler_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DirectoryContentsTest) {
  RunTestURL("foreground/js/directory_contents_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DirectoryTreeTest) {
  RunTestURL("foreground/js/ui/directory_tree_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DriveSyncHandlerTest) {
  RunTestURL("background/js/drive_sync_handler_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DuplicateFinderTest) {
  RunTestURL("background/js/duplicate_finder_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ExifParser) {
  RunTestURL("foreground/js/metadata/exif_parser_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ExternalMetadataProvider) {
  RunTestURL(
      "foreground/js/metadata/external_metadata_provider_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileListModel) {
  RunTestURL("foreground/js/file_list_model_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileListSelectionModelTest) {
  RunTestURL("foreground/js/ui/file_list_selection_model_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileManagerCommandsTest) {
  RunTestURL("foreground/js/file_manager_commands_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileManagerDialogBaseTest) {
  RunTestURL("foreground/js/ui/file_manager_dialog_base_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileOperationHandlerTest) {
  RunTestURL("background/js/file_operation_handler_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileOperationManagerTest) {
  RunTestURL("background/js/file_operation_manager_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesAppEntryTypes) {
  RunTestURL("common/js/files_app_entry_types_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesDisplayPanel) {
  RunTestURL("foreground/elements/files_xf_elements_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesMessage) {
  RunTestURL("foreground/elements/files_message_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesPasswordDialog) {
  RunTestURL("foreground/elements/files_password_dialog_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesToast) {
  RunTestURL("foreground/elements/files_toast_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesToolTip) {
  RunTestURL("foreground/elements/files_tooltip_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileSystemMetadataProvider) {
  RunTestURL(
      "foreground/js/metadata/"
      "file_system_metadata_provider_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTableList) {
  RunTestURL("foreground/js/ui/file_table_list_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTableTest) {
  RunTestURL("foreground/js/ui/file_table_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTapHandler) {
  RunTestURL("foreground/js/ui/file_tap_handler_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTasks) {
  RunTestURL("foreground/js/file_tasks_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTransferController) {
  RunTestURL("foreground/js/file_transfer_controller_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTypeFiltersController) {
  RunTestURL("foreground/js/file_type_filters_controller_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileType) {
  RunTestURL("common/js/file_type_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(CanvasFileManagerJsTest, ImageOrientation) {
  RunTestURL("foreground/js/metadata/image_orientation_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImportControllerTest) {
  RunTestURL("foreground/js/import_controller_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImporterCommonTest) {
  RunTestURL("common/js/importer_common_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImportHistoryTest) {
  RunTestURL("background/js/import_history_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, InstallLinuxPackageDialogTest) {
  RunTestURL(
      "foreground/js/ui/install_linux_package_dialog_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ListThumbnailLoader) {
  RunTestURL("foreground/js/list_thumbnail_loader_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, LRUCacheTest) {
  RunTestURL("common/js/lru_cache_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MediaImportHandlerTest) {
  RunTestURL("background/js/media_import_handler_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MediaScannerTest) {
  RunTestURL("background/js/media_scanner_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataCacheItem) {
  RunTestURL("foreground/js/metadata/metadata_cache_item_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataCacheSet) {
  RunTestURL("foreground/js/metadata/metadata_cache_set_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataModel) {
  RunTestURL("foreground/js/metadata/metadata_model_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataProxyTest) {
  RunTestURL("background/js/metadata_proxy_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MountMetricsTest) {
  RunTestURL("background/js/mount_metrics_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MultiMenu) {
  RunTestURL("foreground/js/ui/multi_menu_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MultiMetadataProvider) {
  RunTestURL(
      "foreground/js/metadata/multi_metadata_provider_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, NavigationListModelTest) {
  RunTestURL("foreground/js/navigation_list_model_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ProvidersModel) {
  RunTestURL("foreground/js/providers_model_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, SpinnerController) {
  RunTestURL("foreground/js/spinner_controller_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TaskController) {
  RunTestURL("foreground/js/task_controller_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TaskQueueTest) {
  RunTestURL("background/js/task_queue_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ThumbnailLoader) {
  RunTestURL("foreground/js/thumbnail_loader_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ThumbnailModel) {
  RunTestURL("foreground/js/metadata/thumbnail_model_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Trash) {
  RunTestURL("background/js/trash_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, UtilTest) {
  RunTestURL("common/js/util_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, VolumeManagerTest) {
  RunTestURL("background/js/volume_manager_unittest.m_gen.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, VolumeManagerTypesTest) {
  RunTestURL("common/js/volume_manager_types_unittest.m_gen.html");
}
