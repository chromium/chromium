// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_FILE_DOWNLOADER_H_
#define CHROME_BROWSER_NET_FILE_DOWNLOADER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

class GURL;

// Helper class to download a file from a given URL and store it in a local
// file. If |overwrite| is true, any existing file will be overwritten;
// otherwise if the local file already exists, this will report success without
// downloading anything.
class FileDownloader {
 public:
  enum Result {
    // The file was successfully downloaded.
    DOWNLOADED,
    // A local file at the given path already existed and was kept.
    EXISTS,
    // Downloading failed.
    FAILED
  };
  using DownloadFinishedCallback = base::OnceCallback<void(Result)>;

  // Directly starts the download (if necessary) and runs |callback| when done.
  // If the instance is destroyed before it is finished, |callback| is not run.
  FileDownloader(
      const GURL& url,
      const base::FilePath& path,
      bool overwrite,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      DownloadFinishedCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~FileDownloader();

  static bool IsSuccess(Result result) { return result != FAILED; }

 private:
  void OnSimpleDownloadComplete(base::FilePath response_path);

  void OnFileExistsCheckDone(bool exists);

  void OnFileMoveDone(bool success);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  DownloadFinishedCallback callback_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::FilePath local_path_;

  base::WeakPtrFactory<FileDownloader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FileDownloader);
};

#endif  // CHROME_BROWSER_NET_FILE_DOWNLOADER_H_
