// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_TEST_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_TEST_HELPER_H_

namespace enterprise_connectors {

// Expected url's for each of the Box mini classes
extern const char kFileSystemBoxFindFolderUrl[];
extern const char kFileSystemBoxCreateFolderUrl[];
extern const char kFileSystemBoxWholeFileUploadUrl[];

// Expected responses for calls to Box endpoints; used for
// network::TestURLLoaderFactory Expected response from
// kFileSystemBoxFindFolderUrl
extern const char kFileSystemBoxFindFolderResponseBody[];
// Expected folder id extracted from kFileSystemBoxFindFolderResponseBody
extern const char kFileSystemBoxFindFolderResponseFolderId[];
// Expected response from kFileSystemBoxFindFolderUrl when there is no matching
// folder
extern const char kFileSystemBoxFindFolderResponseEmptyEntriesList[];
// Expected response from kFileSystemBoxCreateFolderUrl
extern const char kFileSystemBoxCreateFolderResponseBody[];
// Expected folder id extracted from kFileSystemBoxCreateFolderResponseBody
extern const char kFileSystemBoxCreateFolderResponseFolderId[];

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_TEST_HELPER_H_
