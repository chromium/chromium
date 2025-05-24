// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_BAGUETTE_DOWNLOAD_H_
#define CHROME_BROWSER_ASH_CROSTINI_BAGUETTE_DOWNLOAD_H_

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "services/network/public/cpp/mutable_network_traffic_annotation_tag_mojom_traits.h"
#include "url/gurl.h"

class PrefService;
class Profile;
namespace network {
class SimpleURLLoader;
}  // namespace network

namespace crostini {

extern const net::NetworkTrafficAnnotationTag kBaguetteTrafficAnnotation;

// Exposed for testing.
std::string Sha256FileForTesting(const base::FilePath& path);

// Wrapper class to manage the lifetime of a download for a Baguette installer.
// Only good for a single download. Deleting the instance will cancel an
// in-progress and delete any downloaded files.
class BaguetteDownload {
 public:
  virtual ~BaguetteDownload() = default;

  virtual void StartDownload(
      Profile* profile,
      GURL url,
      base::OnceCallback<void(base::FilePath path, std::string sha256)>
          callback) = 0;
};

class SimpleURLLoaderDownload : public BaguetteDownload {
 public:
  explicit SimpleURLLoaderDownload(PrefService& local_state);

  void StartDownload(
      Profile* profile,
      GURL url,
      base::OnceCallback<void(base::FilePath path, std::string sha256)>
          callback) override;

  void SetPostDeletionCallbackForTesting(base::OnceClosure closure) {
    post_deletion_closure_for_testing_ = std::move(closure);
  }

  ~SimpleURLLoaderDownload() override;

 private:
  void Download(Profile* profile, std::unique_ptr<base::ScopedTempDir> dir);
  void Finished(base::FilePath path);

  const raw_ref<PrefService> local_state_;
  GURL url_;
  std::unique_ptr<base::ScopedTempDir> scoped_temp_dir_;
  base::OnceCallback<void(base::FilePath path, std::string sha256)> callback_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  base::OnceClosure post_deletion_closure_for_testing_;

  base::WeakPtrFactory<SimpleURLLoaderDownload> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_BAGUETTE_DOWNLOAD_H_
