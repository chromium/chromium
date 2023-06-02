// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_download.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace bruschetta {

const net::NetworkTrafficAnnotationTag kBruschettaTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("bruschetta_installer_download",
                                        R"(
      semantics {
        sender: "Bruschetta VM Installer",
        description: "Request sent to download firmware and VM image for "
          "a Bruschetta VM, which allows the user to run the VM."
        trigger: "User installing a Bruschetta VM"
        internal {
          contacts {
            email: "clumptini+oncall@google.com"
          }
        }
        user_data: {
          type: ACCESS_TOKEN
        }
        data: "Request to download Bruschetta firmware and VM image. "
          "Sends cookies associated with the source to authenticate the user."
        destination: WEBSITE
        last_reviewed: "2023-01-09"
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        chrome_policy {
          BruschettaVMConfiguration {
            BruschettaVMConfiguration: "{}"
          }
        }
      }
    )");

namespace {

std::unique_ptr<base::ScopedTempDir> MakeTempDir() {
  auto dir = std::make_unique<base::ScopedTempDir>();
  CHECK(dir->CreateUniqueTempDir());
  return dir;
}

// Calculates the sha256 hash of the file at `path` incrementally i.e. without
// loading the entire thing into memory at once. Blocking.
std::string Sha256File(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return "";
  }

  std::unique_ptr<crypto::SecureHash> ctx(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  const size_t kReadBufferSize = 4096;
  char buffer[kReadBufferSize];
  while (true) {
    int count = file.ReadAtCurrentPos(buffer, kReadBufferSize);

    // Treat EOF the same as any other error, stop reading and return the hash
    // of what we read. If there was a disk error or something we'll end up with
    // an invalid hash, same as if the file were truncated.
    if (count <= 0) {
      break;
    }
    ctx->Update(buffer, count);
  }

  char digest_bytes[crypto::kSHA256Length];
  ctx->Finish(digest_bytes, crypto::kSHA256Length);

  return base::HexEncode(digest_bytes, crypto::kSHA256Length);
}

}  // namespace

std::string Sha256FileForTesting(const base::FilePath& path) {
  return Sha256File(path);
}

std::unique_ptr<SimpleURLLoaderDownload> SimpleURLLoaderDownload::StartDownload(
    Profile* profile,
    GURL url,
    base::OnceCallback<void(base::FilePath path, std::string sha256)>
        callback) {
  return base::WrapUnique(new SimpleURLLoaderDownload(profile, std::move(url),
                                                      std::move(callback)));
}

SimpleURLLoaderDownload::SimpleURLLoaderDownload(
    Profile* profile,
    GURL url,
    base::OnceCallback<void(base::FilePath path, std::string sha256)> callback)
    : profile_(profile), url_(std::move(url)), callback_(std::move(callback)) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&MakeTempDir),
      base::BindOnce(&SimpleURLLoaderDownload::Download,
                     weak_ptr_factory_.GetWeakPtr()));
}

SimpleURLLoaderDownload::~SimpleURLLoaderDownload() {
  auto seq = base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  seq->DeleteSoon(FROM_HERE, std::move(scoped_temp_dir_));
  if (post_deletion_closure_for_testing_) {
    seq->PostTask(FROM_HERE, std::move(post_deletion_closure_for_testing_));
  }
}

void SimpleURLLoaderDownload::Download(
    std::unique_ptr<base::ScopedTempDir> dir) {
  scoped_temp_dir_ = std::move(dir);
  auto path = scoped_temp_dir_->GetPath().Append("download");
  auto req = std::make_unique<network::ResourceRequest>();
  req->url = url_;
  loader_ = network::SimpleURLLoader::Create(std::move(req),
                                             kBruschettaTrafficAnnotation);
  auto factory = profile_->GetDefaultStoragePartition()
                     ->GetURLLoaderFactoryForBrowserProcess();
  loader_->DownloadToFile(factory.get(),
                          base::BindOnce(&SimpleURLLoaderDownload::Finished,
                                         weak_ptr_factory_.GetWeakPtr()),
                          std::move(path));
}

void SimpleURLLoaderDownload::Finished(base::FilePath path) {
  if (path.empty()) {
    LOG(ERROR) << "Download failed";
    std::move(callback_).Run(path, "");
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&Sha256File, path),
      base::BindOnce(std::move(callback_), path));
}

}  // namespace bruschetta
