// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/test/spellcheck_content_browser_client.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace spellcheck {

SpellCheckContentBrowserClient::SpellCheckContentBrowserClient() {}
SpellCheckContentBrowserClient::~SpellCheckContentBrowserClient() {}

void SpellCheckContentBrowserClient::OverrideOnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& name,
    mojo::ScopedMessagePipeHandle* handle) {
  if (name != spellcheck::mojom::SpellCheckPanelHost::Name_)
    return;

  spellcheck::mojom::SpellCheckPanelHostRequest request(std::move(*handle));

  // Override the default SpellCheckHost interface.
  auto ui_task_runner = base::CreateSingleThreadTaskRunnerWithTraits(
      {content::BrowserThread::UI});
  ui_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SpellCheckContentBrowserClient::BindSpellCheckPanelHostRequest,
          base::Unretained(this), base::Passed(&request), remote_info));
}

SpellCheckMockPanelHost*
SpellCheckContentBrowserClient::GetSpellCheckMockPanelHostForProcess(
    content::RenderProcessHost* render_process_host) const {
  for (const auto& host : hosts_) {
    if (host->process_host() == render_process_host)
      return host.get();
  }
  return nullptr;
}

void SpellCheckContentBrowserClient::RunUntilBind() {
  base::RunLoop run_loop;
  quit_on_bind_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void SpellCheckContentBrowserClient::BindSpellCheckPanelHostRequest(
    spellcheck::mojom::SpellCheckPanelHostRequest request,
    const service_manager::BindSourceInfo& source_info) {
  service_manager::Identity renderer_identity(
      content::mojom::kRendererServiceName, source_info.identity.user_id(),
      source_info.identity.instance());
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromRendererIdentity(renderer_identity);
  auto spell_check_panel_host =
      std::make_unique<SpellCheckMockPanelHost>(render_process_host);
  spell_check_panel_host->SpellCheckPanelHostRequest(std::move(request));
  hosts_.push_back(std::move(spell_check_panel_host));
  std::move(quit_on_bind_closure_).Run();
}
}  // namespace spellcheck
