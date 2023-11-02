// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"

#include <set>
#include <string>

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "components/drive/drive_api_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {
namespace test_util {

void ExpectEquivalentDetails(const FileDetails& left,
                             const FileDetails& right) {
  std::set<std::string> parents;
  for (int i = 0; i < left.parent_folder_ids_size(); ++i)
    EXPECT_TRUE(parents.insert(left.parent_folder_ids(i)).second);

  for (int i = 0; i < right.parent_folder_ids_size(); ++i)
    EXPECT_EQ(1u, parents.erase(left.parent_folder_ids(i)));
  EXPECT_TRUE(parents.empty());

  EXPECT_EQ(left.title(), right.title());
  EXPECT_EQ(left.file_kind(), right.file_kind());
  EXPECT_EQ(left.md5(), right.md5());
  EXPECT_EQ(left.etag(), right.etag());
  EXPECT_EQ(left.creation_time(), right.creation_time());
  EXPECT_EQ(left.modification_time(), right.modification_time());
  EXPECT_EQ(left.missing(), right.missing());
  EXPECT_EQ(left.change_id(), right.change_id());
}

void ExpectEquivalentMetadata(const FileMetadata& left,
                              const FileMetadata& right) {
  EXPECT_EQ(left.file_id(), right.file_id());
  ExpectEquivalentDetails(left.details(), right.details());
}

void ExpectEquivalentTrackers(const FileTracker& left,
                              const FileTracker& right) {
  EXPECT_EQ(left.tracker_id(), right.tracker_id());
  EXPECT_EQ(left.parent_tracker_id(), right.parent_tracker_id());
  EXPECT_EQ(left.file_id(), right.file_id());
  EXPECT_EQ(left.app_id(), right.app_id());
  EXPECT_EQ(left.tracker_kind(), right.tracker_kind());
  ExpectEquivalentDetails(left.synced_details(), right.synced_details());
  EXPECT_EQ(left.dirty(), right.dirty());
  EXPECT_EQ(left.active(), right.active());
  EXPECT_EQ(left.needs_folder_listing(), right.needs_folder_listing());
}

std::unique_ptr<FileMetadata> CreateFolderMetadata(const std::string& file_id,
                                                   const std::string& title) {
  FileDetails details;
  details.set_title(title);
  details.set_file_kind(FILE_KIND_FOLDER);
  details.set_missing(false);

  std::unique_ptr<FileMetadata> metadata(new FileMetadata);
  metadata->set_file_id(file_id);
  *metadata->mutable_details() = details;

  return metadata;
}

std::unique_ptr<FileMetadata> CreateFileMetadata(const std::string& file_id,
                                                 const std::string& title,
                                                 const std::string& md5) {
  FileDetails details;
  details.set_title(title);
  details.set_file_kind(FILE_KIND_FILE);
  details.set_missing(false);
  details.set_md5(md5);

  std::unique_ptr<FileMetadata> metadata(new FileMetadata);
  metadata->set_file_id(file_id);
  *metadata->mutable_details() = details;

  return metadata;
}

std::unique_ptr<FileTracker> CreateTracker(const FileMetadata& metadata,
                                           int64_t tracker_id,
                                           const FileTracker* parent_tracker) {
  std::unique_ptr<FileTracker> tracker(new FileTracker);
  tracker->set_tracker_id(tracker_id);
  int64_t parent_id =
      parent_tracker ? parent_tracker->tracker_id() : kInvalidTrackerID;
  tracker->set_parent_tracker_id(parent_id);
  tracker->set_file_id(metadata.file_id());
  if (parent_tracker)
    tracker->set_app_id(parent_tracker->app_id());
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  *tracker->mutable_synced_details() = metadata.details();
  tracker->set_dirty(false);
  tracker->set_active(true);
  tracker->set_needs_folder_listing(false);
  return tracker;
}

std::unique_ptr<FileTracker> CreatePlaceholderTracker(
    const std::string& file_id,
    int64_t tracker_id,
    const FileTracker* parent_tracker) {
  std::unique_ptr<FileTracker> tracker(new FileTracker);
  tracker->set_tracker_id(tracker_id);
  if (parent_tracker)
    tracker->set_parent_tracker_id(parent_tracker->tracker_id());
  tracker->set_file_id(file_id);
  if (parent_tracker)
    tracker->set_app_id(parent_tracker->app_id());
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  tracker->set_dirty(true);
  tracker->set_active(false);
  tracker->set_needs_folder_listing(false);
  return tracker;
}

FileResourceKind GetFileResourceKind(
    const google_apis::FileResource& resource) {
  if (resource.IsDirectory())
    return RESOURCE_KIND_FOLDER;
  else
    return RESOURCE_KIND_FILE;
}

}  // namespace test_util
}  // namespace drive_backend
}  // namespace sync_file_system
