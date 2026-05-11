// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_agent_host.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"
#include "url/origin.h"

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

IndigoAgentHost::IndigoAgentHost(content::Page& page)
    : content::PageUserData<IndigoAgentHost>(page) {}

IndigoAgentHost::~IndigoAgentHost() = default;

bool IndigoAgentHost::Invoke() {
  if (injection_state_ == InjectionState::kInjected) {
    GetAgent().Invoke(base::DoNothing());
    return true;
  }

  if (injection_state_ == InjectionState::kInjecting) {
    pending_invoke_count_++;
    return true;
  }

  std::optional<base::FilePath> script_path = IndigoService::GetScriptPath();
  if (!script_path.has_value()) {
    return false;
  }

  injection_state_ = InjectionState::kInjecting;
  pending_invoke_count_++;

  GURL script_url = net::FilePathToFileURL(*script_path);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFileToStringSync, *script_path),
      base::BindOnce(&IndigoAgentHost::OnScriptLoaded,
                     weak_factory_.GetWeakPtr(), std::move(script_url)));
  return true;
}

void IndigoAgentHost::OnScriptLoaded(
    const GURL& script_url,
    std::optional<std::string> script_content) {
  if (!script_content.has_value()) {
    LOG(ERROR) << "Failed to load Indigo script.";
    injection_state_ = InjectionState::kNotInjected;
    pending_invoke_count_ = 0;
    return;
  }

  GetAgent().InjectScript(script_content.value(), script_url, url::Origin(),
                          receiver_.BindNewEndpointAndPassRemote(),
                          base::DoNothing());

  injection_state_ = InjectionState::kInjected;
  while (pending_invoke_count_ > 0) {
    GetAgent().Invoke(base::DoNothing());
    pending_invoke_count_--;
  }
}

void IndigoAgentHost::StartImageReplacement(
    mojo::PendingRemote<blink::mojom::ImageReplacement> replacement,
    bool is_primary,
    StartImageReplacementCallback callback) {
  auto* manager = IndigoImageReplacementManager::GetOrCreateForPage(page());
  manager->RegisterImageReplacement(std::move(replacement), is_primary);
  std::move(callback).Run();
}

chrome::mojom::IndigoAgent& IndigoAgentHost::GetAgent() {
  if (!agent_.is_bound()) {
    this->page()
        .GetMainDocument()
        .GetRemoteAssociatedInterfaces()
        ->GetInterface(&agent_);
  }
  return *agent_.get();
}

PAGE_USER_DATA_KEY_IMPL(IndigoAgentHost);

}  // namespace indigo
