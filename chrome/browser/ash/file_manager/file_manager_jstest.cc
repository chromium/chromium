// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_jstest_base.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

class FileManagerJsTest : public FileManagerJsTestBase {
 protected:
  FileManagerJsTest()
      : FileManagerJsTestBase(
            base::FilePath(FILE_PATH_LITERAL("file_manager"))) {}
};

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ActionsModelTest) {
  RunTestURL("foreground/js/actions_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ActionsSubmenuTest) {
  RunTestURL("foreground/js/ui/actions_submenu_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ArrayDataModel) {
  RunTestURL("common/js/array_data_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, BannerController) {
  RunTestURL("foreground/js/banner_controller_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, BannerDlp) {
  RunTestURL("foreground/js/ui/banners/dlp_restricted_banner_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, BannerUtil) {
  RunTestURL("foreground/js/banner_util_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, BannerEducational) {
  RunTestURL("foreground/js/ui/banners/educational_banner_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, BannerState) {
  RunTestURL("foreground/js/ui/banners/state_banner_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, BannerWarning) {
  RunTestURL("foreground/js/ui/banners/warning_banner_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Command) {
  RunTestURL("foreground/js/ui/command_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ContentMetadataProvider) {
  RunTestURL("foreground/js/metadata/content_metadata_provider_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DlpMetadataProvider) {
  RunTestURL("foreground/js/metadata/dlp_metadata_provider_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ContextMenuHandler) {
  RunTestURL("foreground/js/ui/context_menu_handler_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Crostini) {
  RunTestURL("background/js/crostini_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DirectoryContentsTest) {
  RunTestURL("foreground/js/directory_contents_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DirectoryModelTest) {
  RunTestURL("foreground/js/directory_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DriveBulkPinningBanner) {
  RunTestURL("foreground/js/ui/banners/drive_bulk_pinning_banner_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DriveSyncHandlerTest) {
  RunTestURL("background/js/drive_sync_handler_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ExifParser) {
  RunTestURL("foreground/js/metadata/exif_parser_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ExternalMetadataProvider) {
  RunTestURL("foreground/js/metadata/external_metadata_provider_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileListModel) {
  RunTestURL("foreground/js/file_list_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileListSelectionModelTest) {
  RunTestURL("foreground/js/ui/file_list_selection_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileManagerCommandsTest) {
  RunTestURL("foreground/js/file_manager_commands_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileManagerDialogBaseTest) {
  RunTestURL("foreground/js/ui/file_manager_dialog_base_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesAppEntryTypes) {
  RunTestURL("common/js/files_app_entry_types_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesDisplayPanel) {
  RunTestURL("foreground/elements/files_xf_elements_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesToast) {
  RunTestURL("foreground/elements/files_toast_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilesToolTip) {
  RunTestURL("foreground/elements/files_tooltip_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileSystemMetadataProvider) {
  RunTestURL(
      "foreground/js/metadata/"
      "file_system_metadata_provider_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTableList) {
  RunTestURL("foreground/js/ui/file_table_list_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTableTest) {
  RunTestURL("foreground/js/ui/file_table_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTapHandler) {
  RunTestURL("foreground/js/ui/file_tap_handler_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTasks) {
  RunTestURL("foreground/js/file_tasks_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTransferController) {
  RunTestURL("foreground/js/file_transfer_controller_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTypeFiltersController) {
  RunTestURL("foreground/js/file_type_filters_controller_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, AsyncUtil) {
  RunTestURL("common/js/async_util_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileTypesBase) {
  RunTestURL("common/js/file_types_base_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileType) {
  RunTestURL("common/js/file_type_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FilteredVolumeManagerTest) {
  RunTestURL("common/js/filtered_volume_manager_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Grid) {
  RunTestURL("foreground/js/ui/grid_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, InstallLinuxPackageDialogTest) {
  RunTestURL("foreground/js/ui/install_linux_package_dialog_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Id3Parser) {
  RunTestURL("foreground/js/metadata/id3_parser_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, List) {
  RunTestURL("foreground/js/ui/list_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ListSelectionModel) {
  RunTestURL("foreground/js/ui/list_selection_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ListSingleSelectionModel) {
  RunTestURL("foreground/js/ui/list_single_selection_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ListThumbnailLoader) {
  RunTestURL("foreground/js/list_thumbnail_loader_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, LRUCacheTest) {
  RunTestURL("common/js/lru_cache_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Menu) {
  RunTestURL("foreground/js/ui/menu_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataCacheItem) {
  RunTestURL("foreground/js/metadata/metadata_cache_item_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataCacheSet) {
  RunTestURL("foreground/js/metadata/metadata_cache_set_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataModel) {
  RunTestURL("foreground/js/metadata/metadata_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MultiMenu) {
  RunTestURL("foreground/js/ui/multi_menu_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MultiMetadataProvider) {
  RunTestURL("foreground/js/metadata/multi_metadata_provider_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, PathComponent) {
  RunTestURL("foreground/js/path_component_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, PositionUtil) {
  RunTestURL("foreground/js/ui/position_util_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ProvidersModel) {
  RunTestURL("foreground/js/providers_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Splitter) {
  RunTestURL("foreground/js/ui/splitter_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, SpinnerController) {
  RunTestURL("foreground/js/spinner_controller_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Storage) {
  RunTestURL("common/js/storage_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TaskController) {
  RunTestURL("foreground/js/task_controller_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ThumbnailLoader) {
  RunTestURL("foreground/js/thumbnail_loader_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ThumbnailModel) {
  RunTestURL("foreground/js/metadata/thumbnail_model_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TranslationsTest) {
  RunTestURL("common/js/translations_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, UtilTest) {
  RunTestURL("common/js/util_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, VolumeManagerTest) {
  RunTestURL("background/js/volume_manager_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, VolumeManagerTypesTest) {
  RunTestURL("common/js/volume_manager_types_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, RecentDateBucketTest) {
  RunTestURL("common/js/recent_date_bucket_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfBreadcrumbs) {
  RunTestURL("widgets/xf_breadcrumb_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfSearchOptions) {
  RunTestURL("widgets/xf_search_options_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileGridTest) {
  RunTestURL("foreground/js/ui/file_grid_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, EmptyFolderControllerTest) {
  RunTestURL("foreground/js/empty_folder_controller_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ActionsProducer) {
  RunTestURL("lib/actions_producer_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, BaseStore) {
  RunTestURL("lib/base_store_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, Selector) {
  RunTestURL("lib/selector_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksAllEntries) {
  RunTestURL("state/ducks/all_entries_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksAndroidApps) {
  RunTestURL("state/ducks/android_apps_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksDevice) {
  RunTestURL("state/ducks/device_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksDrive) {
  RunTestURL("state/ducks/drive_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksLaunchParams) {
  RunTestURL("state/ducks/launch_params_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksFolderShortcuts) {
  RunTestURL("state/ducks/folder_shortcuts_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksCurrentDirectory) {
  RunTestURL("state/ducks/current_directory_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksNavigation) {
  RunTestURL("state/ducks/navigation_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksSearch) {
  RunTestURL("state/ducks/search_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksUiEntries) {
  RunTestURL("state/ducks/ui_entries_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksVolumes) {
  RunTestURL("state/ducks/volumes_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksBulkPinning) {
  RunTestURL("state/ducks/bulk_pinning_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DucksPreferences) {
  RunTestURL("state/ducks/preferences_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, NudgeContainer) {
  RunTestURL("containers/nudge_container_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, SearchContainer) {
  RunTestURL("containers/search_container_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfBulkPinningDialog) {
  RunTestURL("widgets/xf_bulk_pinning_dialog_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfConflictDialog) {
  RunTestURL("widgets/xf_conflict_dialog_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfCloudPanel) {
  RunTestURL("widgets/xf_cloud_panel_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfDlpRestrictionDetailsDialog) {
  RunTestURL("widgets/xf_dlp_restriction_details_dialog_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfNudge) {
  RunTestURL("widgets/xf_nudge_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfPasswordDialog) {
  RunTestURL("widgets/xf_password_dialog_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfSelect) {
  RunTestURL("widgets/xf_select_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfSplitter) {
  RunTestURL("widgets/xf_splitter_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfTree) {
  RunTestURL("widgets/xf_tree_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfTreeItem) {
  RunTestURL("widgets/xf_tree_item_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, XfIcon) {
  RunTestURL("widgets/xf_icon_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, BreadcrumbContainer) {
  RunTestURL("containers/breadcrumb_container_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, CloudPanelContainer) {
  RunTestURL("containers/cloud_panel_container_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DirectoryTreeContainer) {
  RunTestURL("containers/directory_tree_container_unittest.js");
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, EntryUtils) {
  RunTestURL("common/js/entry_utils_unittest.js");
}

// Rerun some of the tests above, using CrosComponents.
class FileManagerJsCrosComponentsTest : public FileManagerJsTest {
 public:
  void SetUp() override {
    FileManagerJsTest::SetUp();
    scoped_feature_list_.InitWithFeatures({chromeos::features::kCrosComponents},
                                          {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FileManagerJsCrosComponentsTest, BannerEducational) {
  RunTestURL("foreground/js/ui/banners/educational_banner_unittest.js");
}
