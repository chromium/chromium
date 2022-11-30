// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"

namespace enterprise_connectors {
const char kFileSystemBoxGetFileFolderUrl[] = "https://api.box.com/2.0/files";
const char kFileSystemBoxFindFolderUrl[] =
    "https://api.box.com/2.0/search?type=folder&query=ChromeDownloads";
const char kFileSystemBoxCreateFolderUrl[] = "https://api.box.com/2.0/folders";
const char kFileSystemBoxPreflightCheckUrl[] =
    "https://api.box.com/2.0/files/content";
const char kFileSystemBoxDirectUploadUrl[] =
    "https://upload.box.com/api/2.0/files/content";
const char kFileSystemBoxGetUserUrl[] =
    "https://api.box.com/2.0/users/me?fields=enterprise,login,name";

const char kEmptyResponseBody[] = R"({})";

const char kFileSystemBoxClientErrorResponseBodyFormat[] = R"({
  "type": "error",
  "code": "%s",
  "help_url": "http://developers.box.com/docs/#errors",
  "message": "Dummy message",
  "request_id": "abcdef123456",
  "status": %d
})";

// Request id extracted from the generic error response body above.
const char kFileSystemBoxClientErrorResponseRequestId[] = "abcdef123456";

std::string CreateFailureResponse(int http_code, const char* box_error) {
  return base::StringPrintf(kFileSystemBoxClientErrorResponseBodyFormat,
                            box_error, http_code);
}

// For box Get File Folder
const char kFileSystemBoxGetFileFolderFileId[] = "123";
const char kFileSystemBoxGetFileFolderResponseBody[] = R"({
    "id": 12345,
    "parent": {
      "id": 23456
    }
  })";
const char kFileSystemBoxGetFileFolderResponseFolderId[] = "23456";

// For Box Pre-Upload Steps/////////////////////////////////////////////////////

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

// Should match id's above, as it's used to verify extracted folder_id from
// bodies above.
const char kFileSystemBoxFindFolderResponseFolderId[] = "12345";

// This is the folder_id stored for the kFileSystemUploadFolderIdPref key in
// PrefService for FileSystemDownloadControllerWithSavedFolderPrefTest. It is
// intentionally distinct from kFileSystemBoxFindFolderResponseFolderId above
// to identify where the test flow gets the folder_id from.
const char kFileSystemBoxFolderIdInPref[] = "1337";
const char kFileSystemBoxFolderIdInPrefUrl[] =
    "https://app.box.com/folder/1337";

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

const char kFileSystemBoxCreateFolderConflictResponseBody[] = R"({
  "type": "error",
  "status": 409,
  "code": "item_name_in_use",
  "context_info":{
    "conflicts":[{
      "type": "folder",
      "id": "67890",
      "sequence_id": "0",
      "etag": "0",
      "name": "ChromeDownloads"
    }]
  },
  "help_url": "http:\/\/developers.box.com\/docs\/#errors",
  "message": "Item with the same name already exists",
  "request_id": "2i8lbtgtdld0mle3"
})";

// Should match id in kFileSystemBoxCreateFolderResponseBody, as it's used to
// verify extracted folder_id from body above.
const char kFileSystemBoxCreateFolderResponseFolderId[] = "67890";

// For Box Chunked Uploads /////////////////////////////////////////////////////

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

const char kFileSystemBoxChunkedUploadPartResponseBodyFormat[] = R"({
  "part": {
    "offset": %d,
    "part_id": "6F2D3486",
    "sha1": "134b65991ed521fcfe4724b7d814ab8ded5185dc",
    "size": %d
  }
})";

std::string CreateChunkedUploadPartResponse(int offset, int size) {
  return base::StringPrintf(kFileSystemBoxChunkedUploadPartResponseBodyFormat,
                            offset, size);
}

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

// For Box Uploads (both methods) //////////////////////////////////////////////

const char kFileSystemBoxUploadResponseBody[] = R"({
  "entries": [
    {
      "id": 314159,
      "etag": 1,
      "type": "file",
      "sequence_id": 3,
      "name": "Contract.pdf",
      "sha1": "85136C79CBF9FE36BB9D05D0639C70C265C18D37",
      "file_version": {
        "id": 12345,
        "type": "file_version",
        "sha1": "134b65991ed521fcfe4724b7d814ab8ded5185dc"
      },
      "description": "Contract for Q1 renewal",
      "size": 629644,
      "path_collection": {
        "total_count": 1,
        "entries": [
          {
            "id": 12345,
            "etag": 1,
            "type": "folder",
            "sequence_id": 3,
            "name": "Contracts"
          }
        ]
      },
      "created_at": "2012-12-12T10:53:43-08:00",
      "modified_at": "2012-12-12T10:53:43-08:00",
      "trashed_at": "2012-12-12T10:53:43-08:00",
      "purged_at": "2012-12-12T10:53:43-08:00",
      "content_created_at": "2012-12-12T10:53:43-08:00",
      "content_modified_at": "2012-12-12T10:53:43-08:00",
      "created_by": {
        "id": 11446498,
        "type": "user",
        "name": "Aaron Levie",
        "login": "ceo@example.com"
      },
      "modified_by": {
        "id": 11446498,
        "type": "user",
        "name": "Aaron Levie",
        "login": "ceo@example.com"
      },
      "owned_by": {
        "id": 11446498,
        "type": "user",
        "name": "Aaron Levie",
        "login": "ceo@example.com"
      },
      "shared_link": {
        "url": "https://www.box.com/s/vspke7y05sb214wjokpk",
        "download_url": "https://www.box.com/shared/static/rh935iit6ewrmw0unyul.jpeg",
        "vanity_url": "https://acme.app.box.com/v/my_url/",
        "vanity_name": "my_url",
        "access": "open",
        "effective_access": "company",
        "effective_permission": "can_download",
        "unshared_at": "2018-04-13T13:53:23-07:00",
        "is_password_enabled": true,
        "permissions": {
          "can_download": true,
          "can_preview": true
        },
        "download_count": 3,
        "preview_count": 3
      },
      "parent": {
        "id": 12345,
        "etag": 1,
        "type": "folder",
        "sequence_id": 3,
        "name": "Contracts"
      },
      "item_status": "active"
    }
  ],
  "total_count": 1
})";

// File id should match up id extracted from above.
const char kFileSystemBoxUploadResponseFileId[] = "314159";
const char kFileSystemBoxUploadResponseFileUrl[] =
    "https://app.box.com/file/314159";

}  // namespace enterprise_connectors
