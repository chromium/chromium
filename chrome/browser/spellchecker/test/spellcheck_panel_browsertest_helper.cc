// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/test/spellcheck_panel_browsertest_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/spellchecker/spell_check_panel_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace spellcheck {

SpellCheckPanelBrowserTestHelper::SpellCheckPanelBrowserTestHelper() {
  SpellCheckPanelHostImpl::OverrideBinderForTesting(base::BindRepeating(
      &SpellCheckPanelBrowserTestHelper::BindSpellCheckPanelHost,
      base::Unretained(this)));
}

SpellCheckPanelBrowserTestHelper::~SpellCheckPanelBrowserTestHelper() {
  SpellCheckPanelHostImpl::OverrideBinderForTesting(base::NullCallback());
}

SpellCheckMockPanelHost*
SpellCheckPanelBrowserTestHelper::GetSpellCheckMockPanelHostForProcess(
    content::RenderProcessHost* render_process_host) const {
  for (const auto& host : hosts_) {
    if (host->process_host() == render_process_host)
      return host.get();
  }
  return nullptr;
}

void SpellCheckPanelBrowserTestHelper::RunUntilBind() {
  base::RunLoop run_loop;
  quit_on_bind_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void SpellCheckPanelBrowserTestHelper::BindSpellCheckPanelHost(
    int render_process_id,
    mojo::PendingReceiver<spellcheck::mojom::SpellCheckPanelHost> receiver) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  auto spell_check_panel_host =
      std::make_unique<SpellCheckMockPanelHost>(render_process_host);
  spell_check_panel_host->SpellCheckPanelHostRequest(std::move(receiver));
  hosts_.push_back(std::move(spell_check_panel_host));
  std::move(quit_on_bind_closure_).Run();
}
}  // namespace spellcheck
