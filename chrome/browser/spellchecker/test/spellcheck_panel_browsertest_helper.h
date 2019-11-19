// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_TEST_SPELLCHECK_PANEL_BROWSERTEST_HELPER_H_
#define CHROME_BROWSER_SPELLCHECKER_TEST_SPELLCHECK_PANEL_BROWSERTEST_HELPER_H_

#include <vector>

#include "chrome/browser/spellchecker/test/spellcheck_mock_panel_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace spellcheck {

class SpellCheckPanelBrowserTestHelper {
 public:
  SpellCheckPanelBrowserTestHelper();
  ~SpellCheckPanelBrowserTestHelper();

  SpellCheckMockPanelHost* GetSpellCheckMockPanelHostForProcess(
      content::RenderProcessHost* render_process_host) const;

  void RunUntilBind();

 private:
  void BindSpellCheckPanelHost(
      int render_process_id,
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckPanelHost> receiver);

  base::OnceClosure quit_on_bind_closure_;
  std::vector<std::unique_ptr<SpellCheckMockPanelHost>> hosts_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckPanelBrowserTestHelper);
};

}  // namespace spellcheck

#endif  // CHROME_BROWSER_SPELLCHECKER_TEST_SPELLCHECK_PANEL_BROWSERTEST_HELPER_H_
