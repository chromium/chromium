// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_TEST_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_TEST_HELPER_H_

#include <cstddef>
#include <string>

namespace enterprise_connectors {

// Expected url's for each of the Box mini classes for whole file upload.
extern const char kFileSystemBoxFindFolderUrl[];
extern const char kFileSystemBoxCreateFolderUrl[];
extern const char kFileSystemBoxPreflightCheckUrl[];
extern const char kFileSystemBoxDirectUploadUrl[];

// Expected responses for calls to Box endpoints.

// Empty response body.
extern const char kEmptyResponseBody[];
// Expected response from kFileSystemBoxFindFolderUrl.
extern const char kFileSystemBoxFindFolderResponseBody[];
// Expected folder id extracted from above.
extern const char kFileSystemBoxFindFolderResponseFolderId[];
// Expected response from kFileSystemBoxFindFolderUrl when there is no matching
// folder.
extern const char kFileSystemBoxFindFolderResponseEmptyEntriesList[];
// Expected response from kFileSystemBoxCreateFolderUrl.
extern const char kFileSystemBoxCreateFolderResponseBody[];
// Expected folder id extracted from above.
extern const char kFileSystemBoxCreateFolderResponseFolderId[];

// Saved folder id extracted from the kFileSystemUploadFolderIdPref pref.
extern const char kFileSystemBoxFolderIdInPref[];

// For Box Chunked Uploads /////////////////////////////////////////////////////

// Expected url's for each of the Box mini classes for chunked file upload.
extern const char kFileSystemBoxChunkedUploadCreateSessionUrl[];
extern const char kFileSystemBoxChunkedUploadSessionUrl[];
extern const char kFileSystemBoxChunkedUploadCommitUrl[];

extern const char kFileSystemBoxChunkedUploadSha[];

// Expected response from kFileSystemBoxChunkedUploadCreateSessionUrl.
extern const char kFileSystemBoxChunkedUploadCreateSessionResponseBody[];
// Expected part_size extracted from above.
extern const size_t kFileSystemBoxChunkedUploadCreateSessionResponsePartSize;

void GenerateFileContent(size_t fill_part_size,
                         size_t total_file_size,
                         std::string& content);

size_t CalculateExpectedChunkReadCount(size_t file_size, size_t chunk_size);

// For Box Uploads (both methods) //////////////////////////////////////////////

// Expected response from kFileSystemBoxDirectUploadUrl or
// kFileSystemBoxChunkedUploadCommitUrl after successful upload.
extern const char kFileSystemBoxUploadResponseBody[];
// Expected file url extracted from above.
extern const char kFileSystemBoxUploadResponseFileUrl[];
// Expected folder url for the uploaded file with kFileSystemBoxFolderIdInPref.
extern const char kFileSystemBoxUploadResponseFolderUrl[];
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_TEST_HELPER_H_
