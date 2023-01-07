// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_panel_host_impl.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

SpellCheckPanelHostImpl::Binder& GetPanelHostBinderOverride() {
  static base::NoDestructor<SpellCheckPanelHostImpl::Binder> binder;
  return *binder;
}

}  // namespace

SpellCheckPanelHostImpl::SpellCheckPanelHostImpl() = default;

SpellCheckPanelHostImpl::~SpellCheckPanelHostImpl() = default;

// static
void SpellCheckPanelHostImpl::Create(
    int render_process_id,
    mojo::PendingReceiver<spellcheck::mojom::SpellCheckPanelHost> receiver) {
  auto& binder = GetPanelHostBinderOverride();
  if (binder) {
    binder.Run(render_process_id, std::move(receiver));
    return;
  }

  mojo::MakeSelfOwnedReceiver(std::make_unique<SpellCheckPanelHostImpl>(),
                              std::move(receiver));
}

// static
void SpellCheckPanelHostImpl::OverrideBinderForTesting(Binder binder) {
  GetPanelHostBinderOverride() = std::move(binder);
}

void SpellCheckPanelHostImpl::ShowSpellingPanel(bool show) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  spellcheck_platform::ShowSpellingPanel(show);
}

void SpellCheckPanelHostImpl::UpdateSpellingPanelWithMisspelledWord(
    const std::u16string& word) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  spellcheck_platform::UpdateSpellingPanelWithMisspelledWord(word);
}
