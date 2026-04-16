// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_agent_host.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/component_updater/indigo_component_installer.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/browser/indigo/indigo_script_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace indigo {

namespace {
const char kIndigoScriptSwitch[] = "indigo-script";
}  // namespace

IndigoAgentHost::IndigoAgentHost(content::Page& page)
    : content::PageUserData<IndigoAgentHost>(page) {
  Profile* profile = Profile::FromBrowserContext(
      this->page().GetMainDocument().GetBrowserContext());
  script_loader_ = std::make_unique<IndigoScriptLoader>(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

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

  std::string override_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueUTF8(
          kIndigoScriptSwitch);
  std::optional<base::FilePath> component_path =
      component_updater::GetIndigoContentScriptPath();

  if (override_path.empty() && !component_path.has_value()) {
    return false;
  }

  injection_state_ = InjectionState::kInjecting;
  pending_invoke_count_++;

  if (!override_path.empty()) {
    GURL script_url(override_path);
    if (!script_url.is_valid() || !script_url.SchemeIsHTTPOrHTTPS()) {
      script_url =
          net::FilePathToFileURL(base::FilePath::FromUTF8Unsafe(override_path));
    }
    script_loader_->Load(
        override_path, base::BindOnce(&IndigoAgentHost::OnScriptLoaded,
                                      weak_factory_.GetWeakPtr(), script_url));
  } else {
    GURL script_url = net::FilePathToFileURL(*component_path);
    script_loader_->LoadFromFile(
        *component_path,
        base::BindOnce(&IndigoAgentHost::OnScriptLoaded,
                       weak_factory_.GetWeakPtr(), script_url));
  }
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
    StartImageReplacementCallback callback) {
  auto* manager = IndigoImageReplacementManager::GetOrCreateForPage(page());
  manager->RegisterImageReplacement(std::move(replacement));
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
