// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/baguette_download.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace crostini {

const net::NetworkTrafficAnnotationTag kBaguetteTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("baguette_image_download",
                                        R"(
      semantics {
        sender: "Baguette VM Image",
        description: "Request sent to download VM image for a Baguette VM,"
          "which allows the user to run the VM."
        trigger: "User installing a Baguette VM"
        internal {
          contacts {
            email: "clumptini+oncall@google.com"
          }
        }
        user_data: {
          type: ACCESS_TOKEN
        }
        data: "Request to download Baguette VM image."
        destination: WEBSITE
        last_reviewed: "2023-01-09"
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature can not be disabled by settings."
        policy_exception_justification:
          "This feature is required to deliver core user experiences and "
          "cannot be disabled by policy."
      }
    )");

namespace {

std::unique_ptr<base::ScopedTempDir> MakeTempDir() {
  auto dir = std::make_unique<base::ScopedTempDir>();
  CHECK(dir->CreateUniqueTempDir());
  return dir;
}

std::string Sha256File(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return "";
  }

  std::unique_ptr<crypto::SecureHash> ctx(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  std::array<uint8_t, 4096> buffer;
  while (true) {
    std::optional<size_t> read = file.ReadAtCurrentPos(buffer);

    if (read.value_or(0) == 0) {
      break;
    }
    ctx->Update(base::span(buffer).first(*read));
  }

  std::array<uint8_t, crypto::kSHA256Length> digest_bytes;
  ctx->Finish(digest_bytes);
  return base::HexEncode(digest_bytes);
}

}  // namespace

std::string Sha256FileForTesting(const base::FilePath& path) {
  return Sha256File(path);
}

SimpleURLLoaderDownload::SimpleURLLoaderDownload(PrefService& local_state)
    : local_state_(local_state) {}

SimpleURLLoaderDownload::~SimpleURLLoaderDownload() {
  auto seq = base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  seq->DeleteSoon(FROM_HERE, std::move(scoped_temp_dir_));
  if (post_deletion_closure_for_testing_) {
    seq->PostTask(FROM_HERE, std::move(post_deletion_closure_for_testing_));
  }
}

void SimpleURLLoaderDownload::StartDownload(
    Profile* profile,
    GURL url,
    base::OnceCallback<void(base::FilePath path, std::string sha256)>
        callback) {
  DCHECK(url_.is_empty()) << " each instance is single use.";
  url_ = std::move(url);
  callback_ = std::move(callback);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&MakeTempDir),
      base::BindOnce(&SimpleURLLoaderDownload::Download,
                     weak_ptr_factory_.GetWeakPtr(), profile));
}

void SimpleURLLoaderDownload::Download(
    Profile* profile,
    std::unique_ptr<base::ScopedTempDir> dir) {
  scoped_temp_dir_ = std::move(dir);
  auto path = scoped_temp_dir_->GetPath().Append("download");
  auto req = std::make_unique<network::ResourceRequest>();
  req->url = url_;
  req->method = "GET";
  req->load_flags = net::LOAD_DISABLE_CACHE;
  req->credentials_mode = network::mojom::CredentialsMode::kOmit;
  loader_ = network::SimpleURLLoader::Create(std::move(req),
                                             kBaguetteTrafficAnnotation);
  loader_->SetRetryOptions(
      5, network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  loader_->DownloadToFile(g_browser_process->shared_url_loader_factory().get(),
                          base::BindOnce(&SimpleURLLoaderDownload::Finished,
                                         weak_ptr_factory_.GetWeakPtr()),
                          std::move(path));
}

void SimpleURLLoaderDownload::Finished(base::FilePath path) {
  if (path.empty()) {
    LOG(ERROR) << "Download failed: " << net::ErrorToString(loader_->NetError())
               << "(" << loader_->NetError() << ")";
    std::move(callback_).Run(path, "");
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&Sha256File, path),
      base::BindOnce(std::move(callback_), path));
}

}  // namespace crostini
