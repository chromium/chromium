// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_DOWNLOAD_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_DOWNLOAD_H_

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class Profile;
namespace network {
class SimpleURLLoader;
}  // namespace network

namespace bruschetta {

extern const net::NetworkTrafficAnnotationTag kBruschettaTrafficAnnotation;

// Only exposed so unit tests can test it.
std::string Sha256FileForTesting(const base::FilePath& path);

// Wraps SimpleURLLoader to make it even simpler for Bruschetta to use it for
// downloading files.
class SimpleURLLoaderDownload {
 public:
  // Starts downloading the file at `url`, will invoke `callback` upon
  // completion. Either with the path to the downloaded file and a sha256 hash
  // of its contents, or in case of error will run `callback` with an empty
  // path. Destroying the returned download instance will cancel any active
  // downloads and delete any downloaded files.
  static std::unique_ptr<SimpleURLLoaderDownload> StartDownload(
      Profile* profile,
      GURL url,
      base::OnceCallback<void(base::FilePath path, std::string sha256)>
          callback);

  void SetPostDeletionCallbackForTesting(base::OnceClosure closure) {
    post_deletion_closure_for_testing_ = std::move(closure);
  }

  ~SimpleURLLoaderDownload();

 private:
  SimpleURLLoaderDownload(
      Profile* profile,
      GURL url,
      base::OnceCallback<void(base::FilePath path, std::string sha256)>
          callback);
  void Download(std::unique_ptr<base::ScopedTempDir> dir);
  void Finished(base::FilePath path);

  base::raw_ptr<Profile> profile_;
  GURL url_;
  std::unique_ptr<base::ScopedTempDir> scoped_temp_dir_;
  base::OnceCallback<void(base::FilePath path, std::string sha256)> callback_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::OnceClosure post_deletion_closure_for_testing_;

  base::WeakPtrFactory<SimpleURLLoaderDownload> weak_ptr_factory_{this};
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_DOWNLOAD_H_
