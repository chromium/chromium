// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

class FileManagerJsTest : public FileManagerJsTestBase {
 protected:
  FileManagerJsTest() : FileManagerJsTestBase(
      base::FilePath(FILE_PATH_LITERAL("ui/file_manager/file_manager"))) {}
};

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ActionsSubmenuTest) {
  RunGeneratedTest("/foreground/js/ui/actions_submenu_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ActionsModelTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/actions_model_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, NavigationListModelTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/navigation_list_model_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileOperationHandlerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/file_operation_handler_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ProgressCenterItemGroupTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/progress_center_item_group_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DeviceHandlerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/device_handler_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileOperationManagerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/file_operation_manager_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DriveSyncHandlerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/drive_sync_handler_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImporterCommonTest) {
  RunGeneratedTest("/common/js/importer_common_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImportHistoryTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/import_history_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, VolumeManagerTest) {
  RunGeneratedTest("/background/js/volume_manager_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DirectoryTreeTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/ui/directory_tree_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTableTest) {
  RunGeneratedTest("/foreground/js/ui/file_table_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MediaScannerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/media_scanner_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, LRUCacheTest) {
  RunGeneratedTest("/common/js/lru_cache_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MediaImportHandlerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/media_import_handler_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TaskQueueTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/task_queue_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DuplicateFinderTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/duplicate_finder_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImportControllerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/import_controller_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, AsyncUtilTest) {
  RunGeneratedTest("/common/js/async_util_unittest.html");
}

// TODO(sashab): Re-enable when https://crbug.com/842880 is fixed.
IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DISABLED_MetricsTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("common/js/metrics_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TaskController) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/task_controller_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTasks) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/file_tasks_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ThumbnailLoader) {
  RunGeneratedTest("/foreground/js/thumbnail_loader_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataCacheItem) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/metadata_cache_item_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataCacheSet) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/metadata_cache_set_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MultiMetadataProvider) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/multi_metadata_provider_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ListThumbnailLoader) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/list_thumbnail_loader_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileSystemMetadataProvider) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/file_system_metadata_provider_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ExternalMetadataProvider) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/external_metadata_provider_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ContentMetadataProvider) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/content_metadata_provider_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataModel) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/metadata_model_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ThumbnailModel) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/thumbnail_model_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ExifParser) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/exif_parser_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImageOrientation) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/image_orientation_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ProvidersModel) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/providers_model_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesTooltip) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/elements/files_tooltip_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, SpinnerController) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/spinner_controller_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileListModel) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/file_list_model_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTapHandler) {
  RunGeneratedTest("/foreground/js/ui/file_tap_handler_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileListSelectionModelTest) {
  RunGeneratedTest("/foreground/js/ui/file_list_selection_model_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesAppEntryTypes) {
  RunGeneratedTest("/common/js/files_app_entry_types_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, UtilTest) {
  RunGeneratedTest("/common/js/util_unittest.html");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Crostini) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/crostini_unittest.html")));
}
