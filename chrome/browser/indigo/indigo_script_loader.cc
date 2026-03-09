// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_script_loader.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace indigo {

namespace {

std::optional<std::string> ReadFileToStringSync(const base::FilePath& path) {
  std::string content;
  if (base::ReadFileToString(path, &content)) {
    return content;
  }
  return std::nullopt;
}

}  // namespace

IndigoScriptLoader::IndigoScriptLoader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

IndigoScriptLoader::~IndigoScriptLoader() = default;

void IndigoScriptLoader::Load(std::string_view path, LoadCallback callback) {
  if (path.starts_with("http://") || path.starts_with("https://")) {
    LoadFromNetwork(GURL(path), std::move(callback));
  } else {
    LoadFromFile(base::FilePath::FromUTF8Unsafe(path), std::move(callback));
  }
}

void IndigoScriptLoader::LoadFromFile(const base::FilePath& path,
                                      LoadCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadFileToStringSync, path),
      base::BindOnce(&IndigoScriptLoader::OnFileLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IndigoScriptLoader::LoadFromNetwork(const GURL& url,
                                         LoadCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // TODO: b/491080183 - Either remove this or add a proper traffic annotation
  // if it's being kept around.
  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), MISSING_TRAFFIC_ANNOTATION);

  auto* simple_loader_ptr = simple_loader.get();
  simple_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&IndigoScriptLoader::OnNetworkLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(simple_loader)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void IndigoScriptLoader::OnNetworkLoadComplete(
    LoadCallback callback,
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    std::optional<std::string> response_body) {
  std::move(callback).Run(std::move(response_body));
}

void IndigoScriptLoader::OnFileLoadComplete(
    LoadCallback callback,
    std::optional<std::string> content) {
  std::move(callback).Run(std::move(content));
}

}  // namespace indigo
