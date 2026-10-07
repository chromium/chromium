// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_STANDALONE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_STANDALONE_WINDOW_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/window_controller.h"
#include "components/sessions/core/session_id.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class BaseWindow;
}  // namespace ui

namespace extensions {

class Extension;

// A standalone WindowController test double that defaults to having no
// associated BrowserWindowInterface or Browser (both return nullptr unless
// overridden for lookup tests), representing standalone windows hosting a
// single WebContents (such as Document Picture-in-Picture windows).
class TestStandaloneWindowController : public WindowController {
 public:
  TestStandaloneWindowController(ui::BaseWindow* base_window,
                                 Profile* profile,
                                 SessionID session_id,
                                 content::WebContents* web_contents);
  TestStandaloneWindowController(const TestStandaloneWindowController&) =
      delete;
  TestStandaloneWindowController& operator=(
      const TestStandaloneWindowController&) = delete;
  ~TestStandaloneWindowController() override;

  // Allows ExtensionTabUtil::GetTabById (or GetSplitById) to resolve this
  // window during tests while optionally returning nullptr on subsequent
  // GetBrowserWindowInterface() calls once `count` lookups have occurred.
  void SetBrowserWindowInterfaceForLookup(
      BrowserWindowInterface* bwi,
      std::optional<int> count = std::nullopt);

  // WindowController:
  int GetWindowId() const override;
  std::string GetWindowTypeText() const override;
  void SetFullscreenMode(bool is_fullscreen,
                         const GURL& extension_url) const override;
  BrowserWindowInterface* GetBrowserWindowInterface() override;
  content::WebContents* GetActiveTab() const override;
  int GetTabCount() const override;
  content::WebContents* GetWebContentsAt(int i) const override;
  bool IsVisibleToTabsAPIForExtension(
      const Extension* extension,
      bool include_dev_tools_windows) const override;
  base::DictValue CreateWindowValueForExtension(
      const Extension* extension,
      PopulateTabBehavior populate_tab_behavior,
      mojom::ContextType context) const override;
  base::ListValue CreateTabList(const Extension* extension,
                                mojom::ContextType context) const override;
  bool OpenOptionsPage(const Extension* extension,
                       const GURL& url,
                       bool open_in_tab) override;

 private:
  const SessionID session_id_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<BrowserWindowInterface> lookup_bwi_ = nullptr;
  std::optional<int> remaining_bwi_lookups_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_STANDALONE_WINDOW_CONTROLLER_H_
