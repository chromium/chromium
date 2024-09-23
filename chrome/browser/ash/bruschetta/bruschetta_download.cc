// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/bruschetta/bruschetta_download.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/bruschetta/bruschetta_network_context.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
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
  std::array<uint8_t, 4096> buffer;
  while (true) {
    std::optional<size_t> read = file.ReadAtCurrentPos(buffer);

    // Treat EOF the same as any other error, stop reading and return the hash
    // of what we read. If there was a disk error or something we'll end up with
    // an invalid hash, same as if the file were truncated.
    if (read.value_or(0) == 0) {
      break;
    }
    ctx->Update(base::span(buffer).subspan(0, *read));
  }

  std::array<uint8_t, crypto::kSHA256Length> digest_bytes;
  ctx->Finish(digest_bytes);
  return base::HexEncode(digest_bytes);
}

}  // namespace

std::string Sha256FileForTesting(const base::FilePath& path) {
  return Sha256File(path);
}

SimpleURLLoaderDownload::SimpleURLLoaderDownload() = default;

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
  DCHECK(url_.is_empty()) << " each instance is single use";
  url_ = std::move(url);
  callback_ = std::move(callback);
  // We're owned (through a few levels of owning class) by the installer view
  // which won't outlive the profile, so it's safe to pass around the raw
  // pointer.
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
  loader_ = network::SimpleURLLoader::Create(std::move(req),
                                             kBruschettaTrafficAnnotation);
  network_context_ = std::make_unique<BruschettaNetworkContext>(profile);
  loader_->DownloadToFile(network_context_->GetURLLoaderFactory(),
                          base::BindOnce(&SimpleURLLoaderDownload::Finished,
                                         weak_ptr_factory_.GetWeakPtr()),
                          std::move(path));
}

void SimpleURLLoaderDownload::Finished(base::FilePath path) {
  if (path.empty()) {
    LOG(ERROR) << "Download failed: " << net::ErrorToString(loader_->NetError())
               << " (" << loader_->NetError() << ")";
    std::move(callback_).Run(path, "");
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&Sha256File, path),
      base::BindOnce(std::move(callback_), path));
}

}  // namespace bruschetta
