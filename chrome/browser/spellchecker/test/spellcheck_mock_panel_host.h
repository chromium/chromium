// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_TEST_SPELLCHECK_MOCK_PANEL_HOST_H_
#define CHROME_BROWSER_SPELLCHECKER_TEST_SPELLCHECK_MOCK_PANEL_HOST_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace spellcheck {

class SpellCheckMockPanelHost : public spellcheck::mojom::SpellCheckPanelHost {
 public:
  explicit SpellCheckMockPanelHost(content::RenderProcessHost* process_host);

  SpellCheckMockPanelHost(const SpellCheckMockPanelHost&) = delete;
  SpellCheckMockPanelHost& operator=(const SpellCheckMockPanelHost&) = delete;

  ~SpellCheckMockPanelHost() override;

  content::RenderProcessHost* process_host() const { return process_host_; }

  bool SpellingPanelVisible();
  void BindReceiver(
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckPanelHost> receiver);

 private:
  // spellcheck::mojom::SpellCheckPanelHost:
  void ShowSpellingPanel(bool show) override;
  void UpdateSpellingPanelWithMisspelledWord(
      const std::u16string& word) override;

  mojo::ReceiverSet<spellcheck::mojom::SpellCheckPanelHost> receivers_;
  raw_ptr<content::RenderProcessHost> process_host_;
  bool show_spelling_panel_called_ = false;
  bool spelling_panel_visible_ = false;
  base::OnceClosure quit_;
};
}  // namespace spellcheck

#endif  // CHROME_BROWSER_SPELLCHECKER_TEST_SPELLCHECK_MOCK_PANEL_HOST_H_
