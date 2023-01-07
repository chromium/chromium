// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/test/spellcheck_panel_browsertest_helper.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/spellchecker/spell_check_panel_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
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
  auto* spell_check_panel_host =
      GetSpellCheckMockPanelHostForProcess(render_process_host);
  if (!spell_check_panel_host) {
    hosts_.push_back(
        std::make_unique<SpellCheckMockPanelHost>(render_process_host));
    spell_check_panel_host = hosts_.back().get();
  }
  spell_check_panel_host->BindReceiver(std::move(receiver));

  // BindSpellCheckPanelHost() is sometimes invoked as a side-effect of
  // calling spellcheck::SpellCheckMockPanelHost::SpellingPanelVisible(),
  // which does not call RunUntilBind(). See crbug.com/1032617 .
  if (quit_on_bind_closure_) {
    std::move(quit_on_bind_closure_).Run();
  }
}
}  // namespace spellcheck
