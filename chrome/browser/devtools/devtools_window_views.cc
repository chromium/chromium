// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_process_host.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif

using content::WebContents;

// static
void DevToolsWindow::ToggleDevToolsWindow(BrowserWindowInterface* browser,
                                          const DevToolsToggleAction& action,
                                          DevToolsOpenedByAction opened_by) {
  if (action.type() == DevToolsToggleAction::kToggle &&
      browser->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS) {
    browser->GetTabStripModel()->CloseAllTabs();
    return;
  }

  auto* web_contents = browser->GetTabStripModel()->GetActiveWebContents();
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  auto* print_preview_web_contents =
      printing::PrintPreviewDialogController::GetInstance()
          ->GetPrintPreviewForContents(web_contents);
  if (print_preview_web_contents) {
    web_contents = print_preview_web_contents;
  }
#endif

  ToggleDevToolsWindow(web_contents, nullptr,
                       action.type() == DevToolsToggleAction::kInspect, action,
                       "", opened_by);
}

// static
bool DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(
    BrowserWindowInterface* browser) {
  DCHECK(browser->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS);
  // When FastUnloadController is used, devtools frontend will be detached
  // from the browser window at this point which means we've already fired
  // beforeunload.
  if (browser->GetTabStripModel()->empty()) {
    return true;
  }
  DevToolsWindow* window = AsDevToolsWindow(browser);
  if (!window) {
    return false;
  }
  return window->intercepted_page_beforeunload_;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(
    BrowserWindowInterface* browser) {
  DCHECK(browser->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS);
  if (browser->GetTabStripModel()->empty()) {
    return nullptr;
  }
  WebContents* contents = browser->GetTabStripModel()->GetWebContentsAt(0);
  return AsDevToolsWindow(contents);
}

content::KeyboardEventProcessingResult DevToolsWindow::PreHandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window) {
    return inspected_window->PreHandleKeyboardEvent(event);
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool DevToolsWindow::HandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return true;
  }
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  return inspected_window && inspected_window->HandleKeyboardEvent(event);
}

std::unique_ptr<content::EyeDropper> DevToolsWindow::OpenEyeDropper(
    content::RenderFrameHost* render_frame_host,
    content::EyeDropperListener* listener) {
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window) {
    return window->OpenEyeDropper(render_frame_host, listener);
  }
  return nullptr;
}

BrowserWindow* DevToolsWindow::GetInspectedBrowserWindow() {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (!inspected_web_contents) {
    return nullptr;
  }
  Browser* browser = chrome::FindBrowserWithTab(inspected_web_contents);
  return browser ? browser->window() : nullptr;
}

void DevToolsWindow::UpdateBrowserToolbar() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window) {
    inspected_window->UpdateToolbar(nullptr);
  }
}

void DevToolsWindow::UpdateBrowserWindow() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window) {
    inspected_window->UpdateDevTools(GetInspectedWebContents());
  }
}

void DevToolsWindow::RegisterModalDialogManager(
    BrowserWindowInterface* browser) {
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      main_web_contents_);
  web_modal::WebContentsModalDialogManager::FromWebContents(main_web_contents_)
      ->SetDelegate(browser->GetBrowserForMigrationOnly());

  // Observer `browser` destruction/removal to reset `SetDelegate(nullptr)`
  // before the dialog manager's `raw_ptr` becomes dangling.
  if (!browser_list_observation_.IsObserving()) {
    browser_list_observation_.Observe(BrowserList::GetInstance());
  }
}
