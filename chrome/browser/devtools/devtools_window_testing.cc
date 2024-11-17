// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window_testing.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"

namespace {

const char kHarnessScript[] = "Tests.js";

typedef std::vector<DevToolsWindowTesting*> DevToolsWindowTestings;
base::LazyInstance<DevToolsWindowTestings>::Leaky
    g_devtools_window_testing_instances = LAZY_INSTANCE_INITIALIZER;
}

DevToolsWindowTesting::DevToolsWindowTesting(DevToolsWindow* window)
    : devtools_window_(window) {
  DCHECK(window);
  window->close_callback_ =
      base::BindOnce(&DevToolsWindowTesting::WindowClosed, window);
  g_devtools_window_testing_instances.Get().push_back(this);
}

DevToolsWindowTesting::~DevToolsWindowTesting() {
  DevToolsWindowTestings* instances =
      g_devtools_window_testing_instances.Pointer();
  auto it = base::ranges::find(*instances, this);
  CHECK(it != instances->end());
  instances->erase(it);
  if (!close_callback_.is_null())
    std::move(close_callback_).Run();

  // Needed for Chrome_DevToolsADBThread to shut down gracefully in tests.
  ChromeDevToolsManagerDelegate::GetInstance()
      ->ResetAndroidDeviceManagerForTesting();
}

// static
DevToolsWindowTesting* DevToolsWindowTesting::Get(DevToolsWindow* window) {
  DevToolsWindowTesting* testing = DevToolsWindowTesting::Find(window);
  if (!testing)
    testing = new DevToolsWindowTesting(window);
  return testing;
}

// static
DevToolsWindowTesting* DevToolsWindowTesting::Find(DevToolsWindow* window) {
  if (!g_devtools_window_testing_instances.IsCreated())
    return nullptr;
  DevToolsWindowTestings* instances =
      g_devtools_window_testing_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->devtools_window_ == window)
      return *it;
  }
  return nullptr;
}

Browser* DevToolsWindowTesting::browser() {
  return devtools_window_->browser_;
}

content::WebContents* DevToolsWindowTesting::main_web_contents() {
  return devtools_window_->main_web_contents_;
}

content::WebContents* DevToolsWindowTesting::toolbox_web_contents() {
  return devtools_window_->toolbox_web_contents_;
}

void DevToolsWindowTesting::SetInspectedPageBounds(const gfx::Rect& bounds) {
  devtools_window_->SetInspectedPageBounds(bounds);
}

void DevToolsWindowTesting::SetCloseCallback(base::OnceClosure closure) {
  close_callback_ = std::move(closure);
}

void DevToolsWindowTesting::SetOpenNewWindowForPopups(bool value) {
  devtools_window_->SetOpenNewWindowForPopups(value);
}

// static
void DevToolsWindowTesting::WindowClosed(DevToolsWindow* window) {
  DevToolsWindowTesting* testing = DevToolsWindowTesting::Find(window);
  if (testing)
    delete testing;
}

// static
void DevToolsWindowTesting::WaitForDevToolsWindowLoad(DevToolsWindow* window) {
  if (!window) {
    return;
  }
  auto* main_web_contents = window->main_web_contents_.get();
  if (!window->ready_for_test_) {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    window->ready_for_test_callback_ = runner->QuitClosure();
    runner->Run();
  }

  // The window might be removed upon creation.
  // E.g. devtools windows may not be allowed in kiosk mode and killed upon
  // creation..
  if (!DevToolsWindow::IsDevToolsWindow(main_web_contents)) {
    return;
  }

  std::u16string harness = base::UTF8ToUTF16(
      content::DevToolsFrontendHost::GetFrontendResource(kHarnessScript));
  window->main_web_contents_->GetPrimaryMainFrame()->ExecuteJavaScript(
      harness, base::NullCallback());
}

// static
DevToolsWindow* DevToolsWindowTesting::OpenDevToolsWindowSync(
    content::WebContents* inspected_web_contents,
    Profile* profile,
    bool is_docked) {
  std::string settings = is_docked ?
      "{\"isUnderTest\": true, \"currentDockState\":\"\\\"bottom\\\"\"}" :
      "{\"isUnderTest\": true, \"currentDockState\":\"\\\"undocked\\\"\"}";
  scoped_refptr<content::DevToolsAgentHost> agent(
      content::DevToolsAgentHost::GetOrCreateForTab(inspected_web_contents));
  DevToolsWindow::ToggleDevToolsWindow(inspected_web_contents, profile, true,
                                       DevToolsToggleAction::Show(), settings);
  DevToolsWindow* window = DevToolsWindow::FindDevToolsWindow(agent.get());
  WaitForDevToolsWindowLoad(window);
  return window;
}

// static
DevToolsWindow* DevToolsWindowTesting::OpenDevToolsWindowSync(
    content::WebContents* inspected_web_contents,
    bool is_docked) {
  return OpenDevToolsWindowSync(
      inspected_web_contents,
      DevToolsWindow::GetProfileForDevToolsWindow(inspected_web_contents),
      is_docked);
}

// static
DevToolsWindow* DevToolsWindowTesting::OpenDevToolsWindowSync(
    Browser* browser,
    bool is_docked) {
  return OpenDevToolsWindowSync(
      browser->tab_strip_model()->GetActiveWebContents(), is_docked);
}

// static
DevToolsWindow* DevToolsWindowTesting::OpenDevToolsWindowSync(
    Profile* profile,
    scoped_refptr<content::DevToolsAgentHost> agent_host) {
  DevToolsWindow::OpenDevToolsWindow(agent_host, profile,
                                     DevToolsOpenedByAction::kUnknown);
  DevToolsWindow* window = DevToolsWindow::FindDevToolsWindow(agent_host.get());
  WaitForDevToolsWindowLoad(window);
  return window;
}

// static
DevToolsWindow* DevToolsWindowTesting::OpenDiscoveryDevToolsWindowSync(
    Profile* profile) {
  DevToolsWindow* window = DevToolsWindow::OpenNodeFrontendWindow(
      profile, DevToolsOpenedByAction::kUnknown);
  WaitForDevToolsWindowLoad(window);
  return window;
}

// static
void DevToolsWindowTesting::CloseDevToolsWindow(
    DevToolsWindow* window) {
  if (window->is_docked_) {
    window->CloseWindow();
  } else {
    window->browser_->window()->Close();
  }
}

// static
void DevToolsWindowTesting::CloseDevToolsWindowSync(
    DevToolsWindow* window) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  DevToolsWindowTesting::Get(window)->SetCloseCallback(runner->QuitClosure());
  CloseDevToolsWindow(window);
  runner->Run();
}

// DevToolsWindowCreationObserver ---------------------------------------------

DevToolsWindowCreationObserver::DevToolsWindowCreationObserver()
    : creation_callback_(base::BindRepeating(
          &DevToolsWindowCreationObserver::DevToolsWindowCreated,
          base::Unretained(this))) {
  DevToolsWindow::AddCreationCallbackForTest(creation_callback_);
}

DevToolsWindowCreationObserver::~DevToolsWindowCreationObserver() {
  DevToolsWindow::RemoveCreationCallbackForTest(creation_callback_);
}

void DevToolsWindowCreationObserver::Wait() {
  if (!devtools_windows_.empty())
    return;
  runner_ = new content::MessageLoopRunner();
  runner_->Run();
  runner_ = nullptr;
}

void DevToolsWindowCreationObserver::WaitForLoad() {
  Wait();
  if (devtools_window())
    DevToolsWindowTesting::WaitForDevToolsWindowLoad(devtools_window());
}

void DevToolsWindowCreationObserver::DevToolsWindowCreated(
    DevToolsWindow* devtools_window) {
  devtools_windows_.push_back(devtools_window);
  if (runner_.get())
    runner_->QuitClosure().Run();
}

DevToolsWindow* DevToolsWindowCreationObserver::devtools_window() {
  return !devtools_windows_.empty() ? devtools_windows_.back() : nullptr;
}

void DevToolsWindowCreationObserver::CloseAllSync() {
  for (DevToolsWindow* window : devtools_windows_)
    DevToolsWindowTesting::CloseDevToolsWindowSync(window);
  devtools_windows_.clear();
}
