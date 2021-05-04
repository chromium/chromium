// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"

#include "base/check_op.h"

namespace enterprise_connectors {
const char kFileSystemBoxFindFolderUrl[] =
    "https://api.box.com/2.0/search?type=folder&query=ChromeDownloads";
const char kFileSystemBoxCreateFolderUrl[] = "https://api.box.com/2.0/folders";
const char kFileSystemBoxPreflightCheckUrl[] =
    "https://api.box.com/2.0/files/content";
const char kFileSystemBoxDirectUploadUrl[] =
    "https://upload.box.com/api/2.0/files/content";

const char kEmptyResponseBody[] = R"({})";
const char kFileSystemBoxFindFolderResponseBody[] = R"({
    "entries": [
      {
        "id": 12345,
        "etag": 1,
        "type": "folder",
        "sequence_id": 3,
        "name": "ChromeDownloads"
      }
    ]
  })";

// Should match id in kFileSystemBoxFindFolderResponseBody, as it's used to
// verify extracted folder_id from body above.
const char kFileSystemBoxFindFolderResponseFolderId[] = "12345";

// This is the folder_id stored for the kFileSystemUploadFolderIdPref key in
// PrefService for FileSystemDownloadControllerWithSavedFolderPrefTest. It is
// intentionally distinct from kFileSystemBoxFindFolderResponseFolderId above
// to identify where the test flow gets the folder_id from.
const char kFileSystemBoxFolderIdInPref[] = "1337";

const char kFileSystemBoxFindFolderResponseEmptyEntriesList[] = R"({
    "entries": [
        ]
  })";

const char kFileSystemBoxCreateFolderResponseBody[] = R"({
    "id": 67890,
    "type": "folder",
    "content_created_at": "2012-12-12T10:53:43-08:00",
    "content_modified_at": "2012-12-12T10:53:43-08:00",
    "created_at": "2012-12-12T10:53:43-08:00",
    "created_by": {
      "id": 11446498,
      "type": "user",
      "login": "ceo@example.com",
      "name": "Aaron Levie"
    },
    "description": "Files downloaded in Chrome",
    "etag": 1,
    "expires_at": "2012-12-12T10:53:43-08:00",
    "folder_upload_email": {
      "access": "open",
      "email": "upload.Contracts.asd7asd@u.box.com"
    },
    "name": "ChromeDownloads",
    "owned_by": {
      "id": 11446498,
      "type": "user",
      "login": "ceo@example.com",
      "name": "Aaron Levie"
    },
    "parent": {
      "id": 0,
      "type": "folder",
      "etag": 1,
      "name": "",
      "sequence_id": 3
    }
  })";

// Should match id in kFileSystemBoxCreateFolderResponseBody, as it's used to
// verify extracted folder_id from body above.
const char kFileSystemBoxCreateFolderResponseFolderId[] = "67890";

const char kFileSystemBoxChunkedUploadCreateSessionUrl[] =
    "https://upload.box.com/api/2.0/files/upload_sessions";
const char kFileSystemBoxChunkedUploadSessionUrl[] =
    "https://upload.box.com/api/2.0/files/upload_sessions/"
    "F971964745A5CD0C001BBE4E58196BFD";
const char kFileSystemBoxChunkedUploadCommitUrl[] =
    "https://upload.box.com/api/2.0/files/upload_sessions/"
    "F971964745A5CD0C001BBE4E58196BFD/commit";
const char kFileSystemBoxChunkedUploadSha[] = "fpRyg5eVQletdZqEKaFlqwBXJzM";

// Endpoints should match the corresponding const char url's above.
const char kFileSystemBoxChunkedUploadCreateSessionResponseBody[] = R"({
  "id": "F971964745A5CD0C001BBE4E58196BFD",
  "type": "upload_session",
  "num_parts_processed": 455,
  "part_size": 7340032,
  "session_endpoints": {
    "abort": "https://upload.box.com/api/2.0/files/upload_sessions/F971964745A5CD0C001BBE4E58196BFD",
    "commit": "https://upload.box.com/api/2.0/files/upload_sessions/F971964745A5CD0C001BBE4E58196BFD/commit",
    "list_parts": "https://upload.box.com/api/2.0/files/upload_sessions/F971964745A5CD0C001BBE4E58196BFD/parts",
    "log_event": "https://upload.box.com/api/2.0/files/upload_sessions/F971964745A5CD0C001BBE4E58196BFD/log",
    "status": "https://upload.box.com/api/2.0/files/upload_sessions/F971964745A5CD0C001BBE4E58196BFD",
    "upload_part": "https://upload.box.com/api/2.0/files/upload_sessions/F971964745A5CD0C001BBE4E58196BFD"
  },
  "session_expires_at": "2012-12-12T10:53:43-08:00",
  "total_parts": 1000
})";

// Assumes 7MB per part. Should match part_size in
// kFileSystemBoxChunkedUploadCreateSessionResponseBody, as it's used to verify
// extracted part_size from body above.
const size_t kFileSystemBoxChunkedUploadCreateSessionResponsePartSize = 7340032;

void GenerateFileContent(size_t part_size,
                         size_t total_size,
                         std::string& txt) {
  txt.clear();
  txt.reserve(total_size);
  CHECK_LT(total_size, part_size * 26);
  for (char c = 'a'; c <= 'z' && (txt.size() + part_size) < total_size; ++c) {
    txt += std::string(part_size, c);
  }
  txt += std::string(total_size - txt.size(), 'z');
  CHECK_EQ(total_size, txt.size());
}

size_t CalculateExpectedChunkReadCount(size_t file_size, size_t chunk_size) {
  DCHECK_GE(file_size, chunk_size);
  size_t expected_read_count = file_size / chunk_size;
  if (file_size % chunk_size != 0) {
    ++expected_read_count;
  }
  return expected_read_count;
}

}  // namespace enterprise_connectors
