// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/test/spellcheck_mock_panel_host.h"

#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"

namespace spellcheck {

SpellCheckMockPanelHost::SpellCheckMockPanelHost(
    content::RenderProcessHost* process_host)
    : process_host_(process_host) {}

SpellCheckMockPanelHost::~SpellCheckMockPanelHost() {}

bool SpellCheckMockPanelHost::SpellingPanelVisible() {
  if (!show_spelling_panel_called_) {
    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  return spelling_panel_visible_;
}

void SpellCheckMockPanelHost::BindReceiver(
    mojo::PendingReceiver<spellcheck::mojom::SpellCheckPanelHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

// spellcheck::mojom::SpellCheckPanelHost:
void SpellCheckMockPanelHost::ShowSpellingPanel(bool show) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  show_spelling_panel_called_ = true;
  spelling_panel_visible_ = show;
  if (quit_)
    std::move(quit_).Run();
}

void SpellCheckMockPanelHost::UpdateSpellingPanelWithMisspelledWord(
    const std::u16string& word) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}
}  // namespace spellcheck
