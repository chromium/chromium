// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_RESOLVER_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_RESOLVER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
}
namespace storage {
class FileStreamReader;
class FileSystemContext;
}  // namespace storage
class GURL;

namespace chromeos {

// Resolves an externalfile URL to a redirect or a FileStreamReader.
class ExternalFileResolver {
 public:
  using ErrorCallback = base::OnceCallback<void(net::Error)>;
  using RedirectCallback = base::OnceCallback<void(const std::string& mime_type,
                                                   const GURL& redirect_url)>;
  using StreamCallback = base::OnceCallback<void(
      const std::string& mime_type,
      storage::IsolatedContext::ScopedFSHandle isolated_file_system_scope,
      std::unique_ptr<storage::FileStreamReader> stream_reader,
      int64_t size)>;

  explicit ExternalFileResolver(void* profile_id);
  virtual ~ExternalFileResolver();

  // Extracts any extra information needed to open the URL from the request's
  // headers. This should be called before Resolve.
  void ProcessHeaders(const net::HttpRequestHeaders& headers);

  // Resolves |url| to a redirect, or a FileStreamReader that contains the
  // file's content.
  void Resolve(const std::string& method,
               const GURL& url,
               ErrorCallback error_callback,
               RedirectCallback redirect_callback,
               StreamCallback stream_callback);

 private:
  void OnHelperResultObtained(
      net::Error error,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      file_manager::util::FileSystemURLAndHandle isolated_file_system,
      const std::string& mime_type);

  void OnRedirectURLObtained(const GURL& redirect_url);

  void OnFileInfoObtained(base::File::Error error,
                          const base::File::Info& file_info);

  void* profile_id_;
  net::Error range_parse_result_;
  net::HttpByteRange byte_range_;

  ErrorCallback error_callback_;
  RedirectCallback redirect_callback_;
  StreamCallback stream_callback_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  file_manager::util::FileSystemURLAndHandle isolated_file_system_;
  std::string mime_type_;
  base::WeakPtrFactory<ExternalFileResolver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExternalFileResolver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_RESOLVER_H_
