// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_CONTENT_CLIENT_SHELL_BROWSER_MAIN_PARTS_H_
#define ASH_SHELL_CONTENT_CLIENT_SHELL_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {
class BrowserContext;
struct MainFunctionParams;
}  // namespace content

namespace views {
class ViewsDelegate;
}

namespace ash {
class AshTestHelper;

namespace shell {
class ExampleAppListClient;
class ExampleSessionControllerClient;
class ShellNewWindowDelegate;
class WindowWatcher;

class ShellBrowserMainParts : public content::BrowserMainParts {
 public:
  static content::BrowserContext* GetBrowserContext();

  explicit ShellBrowserMainParts(const content::MainFunctionParams& parameters);
  ~ShellBrowserMainParts() override;

  // Overridden from content::BrowserMainParts:
  void PostEarlyInitialization() override;
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  void ToolkitInitialized() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  content::BrowserContext* browser_context() { return browser_context_.get(); }

 private:
  std::unique_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<views::ViewsDelegate> views_delegate_;
  std::unique_ptr<WindowWatcher> window_watcher_;
  std::unique_ptr<ExampleSessionControllerClient>
      example_session_controller_client_;
  std::unique_ptr<ExampleAppListClient> example_app_list_client_;
  std::unique_ptr<ash::AshTestHelper> ash_test_helper_;
  std::unique_ptr<ShellNewWindowDelegate> new_window_delegate_;
  content::MainFunctionParams parameters_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_CONTENT_CLIENT_SHELL_BROWSER_MAIN_PARTS_H_
