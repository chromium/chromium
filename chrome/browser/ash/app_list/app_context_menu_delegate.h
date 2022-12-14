// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_CONTEXT_MENU_DELEGATE_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_CONTEXT_MENU_DELEGATE_H_

namespace app_list {

class AppContextMenuDelegate {
 public:
  // Invoked to execute "Launch" command.
  virtual void ExecuteLaunchCommand(int event_flags) = 0;

 protected:
  virtual ~AppContextMenuDelegate() {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_CONTEXT_MENU_DELEGATE_H_
