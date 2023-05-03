// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"

#include <utility>

#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"

namespace enterprise_connectors {
namespace {

ContentAnalysisSdkManager* sdk_manager_for_testing = nullptr;
}

ContentAnalysisSdkManager::WrappedClient::WrappedClient(
    std::unique_ptr<content_analysis::sdk::Client> client)
    : client_(std::move(client)) {}

ContentAnalysisSdkManager::WrappedClient::~WrappedClient() = default;

// static
ContentAnalysisSdkManager* ContentAnalysisSdkManager::Get() {
  if (sdk_manager_for_testing)
    return sdk_manager_for_testing;
  static base::NoDestructor<ContentAnalysisSdkManager> manager;
  return manager.get();
}

// static
void ContentAnalysisSdkManager::SetManagerForTesting(
    ContentAnalysisSdkManager* manager) {
  sdk_manager_for_testing = manager;
}

ContentAnalysisSdkManager::ContentAnalysisSdkManager() = default;

ContentAnalysisSdkManager::~ContentAnalysisSdkManager() = default;

scoped_refptr<ContentAnalysisSdkManager::WrappedClient>
ContentAnalysisSdkManager::GetClient(
    content_analysis::sdk::Client::Config config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = clients_.find(config);
  if (it != clients_.end())
    return it->second;

  auto client = CreateClient(config);
  if (client) {
    auto wrapped = base::MakeRefCounted<WrappedClient>(std::move(client));
    clients_.insert(std::make_pair(std::move(config), wrapped));
    return wrapped;
  }

  return nullptr;
}

void ContentAnalysisSdkManager::ResetClient(
    const content_analysis::sdk::Client::Config& config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  clients_.erase(config);
}

void ContentAnalysisSdkManager::ResetAllClients() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  clients_.clear();
}

std::unique_ptr<content_analysis::sdk::Client>
ContentAnalysisSdkManager::CreateClient(
    const content_analysis::sdk::Client::Config& config) {
  return content_analysis::sdk::Client::Create(config);
}

}  // namespace enterprise_connectors
