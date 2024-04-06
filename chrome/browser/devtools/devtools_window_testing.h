// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_TESTING_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_TESTING_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "ui/gfx/geometry/rect.h"

class Browser;
class Profile;

namespace content {
class DevToolsAgentHost;
class MessageLoopRunner;
class WebContents;
}

class DevToolsWindowTesting {
 public:
  DevToolsWindowTesting(const DevToolsWindowTesting&) = delete;
  DevToolsWindowTesting& operator=(const DevToolsWindowTesting&) = delete;

  virtual ~DevToolsWindowTesting();

  // The following methods block until DevToolsWindow is completely loaded.
  static DevToolsWindow* OpenDevToolsWindowSync(
      content::WebContents* inspected_web_contents,
      Profile* profile,
      bool is_docked);
  static DevToolsWindow* OpenDevToolsWindowSync(
      content::WebContents* inspected_web_contents,
      bool is_docked);
  static DevToolsWindow* OpenDevToolsWindowSync(
      Browser* browser, bool is_docked);
  static DevToolsWindow* OpenDevToolsWindowSync(
      Profile* profile,
      scoped_refptr<content::DevToolsAgentHost> agent_host);
  static DevToolsWindow* OpenDiscoveryDevToolsWindowSync(Profile* profile);

  // Closes the window like it was user-initiated.
  static void CloseDevToolsWindow(DevToolsWindow* window);
  // Blocks until window is closed.
  static void CloseDevToolsWindowSync(DevToolsWindow* window);

  static DevToolsWindowTesting* Get(DevToolsWindow* window);

  Browser* browser();
  content::WebContents* main_web_contents();
  content::WebContents* toolbox_web_contents();
  void SetInspectedPageBounds(const gfx::Rect& bounds);
  void SetCloseCallback(base::OnceClosure closure);
  void SetOpenNewWindowForPopups(bool value);

 private:
  friend class DevToolsWindow;
  friend class DevToolsWindowCreationObserver;

  explicit DevToolsWindowTesting(DevToolsWindow* window);
  static void WaitForDevToolsWindowLoad(DevToolsWindow* window);
  static void WindowClosed(DevToolsWindow* window);
  static DevToolsWindowTesting* Find(DevToolsWindow* window);

  raw_ptr<DevToolsWindow> devtools_window_;
  base::OnceClosure close_callback_;
};

class DevToolsWindowCreationObserver {
 public:
  DevToolsWindowCreationObserver();

  DevToolsWindowCreationObserver(const DevToolsWindowCreationObserver&) =
      delete;
  DevToolsWindowCreationObserver& operator=(
      const DevToolsWindowCreationObserver&) = delete;

  ~DevToolsWindowCreationObserver();

  using DevToolsWindows =
      std::vector<raw_ptr<DevToolsWindow, VectorExperimental>>;
  const DevToolsWindows& devtools_windows() { return devtools_windows_; }
  DevToolsWindow* devtools_window();

  void Wait();
  void WaitForLoad();
  void CloseAllSync();

 private:
  friend class DevToolsWindow;

  void DevToolsWindowCreated(DevToolsWindow* devtools_window);

  base::RepeatingCallback<void(DevToolsWindow*)> creation_callback_;
  DevToolsWindows devtools_windows_;
  scoped_refptr<content::MessageLoopRunner> runner_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_TESTING_H_
