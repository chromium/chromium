// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_FILE_STREAM_READER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_FILE_STREAM_READER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"

class GURL;

namespace storage {

class FileSystemURL;

}  // namespace storage

namespace arc {

// FileStreamReader implementation for ARC documents provider file system.
// It actually delegates operations to ArcContentFileSystemFileStreamReader.
// TODO(crbug.com/678886): Write unit tests.
class ArcDocumentsProviderFileStreamReader : public storage::FileStreamReader {
 public:
  ArcDocumentsProviderFileStreamReader(const storage::FileSystemURL& url,
                                       int64_t offset);
  ~ArcDocumentsProviderFileStreamReader() override;

  // storage::FileStreamReader override:
  int Read(net::IOBuffer* buffer,
           int buffer_length,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  void OnResolveToContentUrl(const GURL& content_url);
  void RunPendingRead(scoped_refptr<net::IOBuffer> buffer,
                      int buffer_length,
                      net::CompletionOnceCallback callback);
  void RunPendingGetLength(net::Int64CompletionOnceCallback callback);

  const int64_t offset_;
  bool content_url_resolved_;
  std::unique_ptr<storage::FileStreamReader> underlying_reader_;
  std::vector<base::OnceClosure> pending_operations_;

  base::WeakPtrFactory<ArcDocumentsProviderFileStreamReader> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ArcDocumentsProviderFileStreamReader);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_FILE_STREAM_READER_H_
