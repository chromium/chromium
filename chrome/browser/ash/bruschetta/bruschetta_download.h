// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_DOWNLOAD_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_DOWNLOAD_H_

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class Profile;
namespace network {
class SimpleURLLoader;
}  // namespace network

namespace bruschetta {
class BruschettaNetworkContext;

extern const net::NetworkTrafficAnnotationTag kBruschettaTrafficAnnotation;

// Only exposed so unit tests can test it.
std::string Sha256FileForTesting(const base::FilePath& path);

// Wrapper class to manage the lifetime of a download for the Bruschetta
// installer. Only good for a single download (i.e. must only call StartDownload
// once). Deleting this instance will cancel an in-progress download and delete
// any downloaded files.
class BruschettaDownload {
 public:
  virtual ~BruschettaDownload() = default;

  // Starts downloading the file at `url`, will invoke `callback` upon
  // completion. Either with the path to the downloaded file and a sha256 hash
  // of its contents, or in case of error will run `callback` with an empty
  // path.
  virtual void StartDownload(
      Profile* profile,
      GURL url,
      base::OnceCallback<void(base::FilePath path, std::string sha256)>
          callback) = 0;
};

// Wraps SimpleURLLoader to make it even simpler for Bruschetta to use it for
// downloading files.
class SimpleURLLoaderDownload : public BruschettaDownload {
 public:
  SimpleURLLoaderDownload();
  // Starts downloading the file at `url`, will invoke `callback` upon
  // completion. Either with the path to the downloaded file and a sha256 hash
  // of its contents, or in case of error will run `callback` with an empty
  // path.
  void StartDownload(
      Profile* profile,
      GURL url,
      base::OnceCallback<void(base::FilePath path, std::string sha256)>
          callback) override;

  void SetPostDeletionCallbackForTesting(base::OnceClosure closure) {
    post_deletion_closure_for_testing_ = std::move(closure);
  }

  // Cancel a download if in-progress and delete any downloaded files.
  ~SimpleURLLoaderDownload() override;

 private:
  SimpleURLLoaderDownload(
      Profile* profile,
      GURL url,
      base::OnceCallback<void(base::FilePath path, std::string sha256)>
          callback);
  void Download(Profile* profile, std::unique_ptr<base::ScopedTempDir> dir);
  void Finished(base::FilePath path);

  GURL url_;
  std::unique_ptr<base::ScopedTempDir> scoped_temp_dir_;
  base::OnceCallback<void(base::FilePath path, std::string sha256)> callback_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::OnceClosure post_deletion_closure_for_testing_;
  std::unique_ptr<BruschettaNetworkContext> network_context_;

  base::WeakPtrFactory<SimpleURLLoaderDownload> weak_ptr_factory_{this};
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_DOWNLOAD_H_
