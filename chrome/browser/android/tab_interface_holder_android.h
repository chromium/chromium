// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_INTERFACE_HOLDER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_INTERFACE_HOLDER_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "components/tabs/public/tab_interface_holder.h"

class TabAndroid;

namespace tabs {
class TabInterface;
}  // namespace tabs

// Wraps a WeakPtr to a `TabAndroid` in a unique_ptr for compatibility with
// tab collections. Tab lifecycle on Android is managed via a ref counted Java
// object that explicitly destroys the C++ component as part of a destroy
// method.
class TabInterfaceHolderAndroid : public tabs::TabInterfaceHolder {
 public:
  explicit TabInterfaceHolderAndroid(TabAndroid* tab_android);
  ~TabInterfaceHolderAndroid() override;

  TabInterfaceHolderAndroid(const TabInterfaceHolderAndroid&) = delete;
  void operator=(const TabInterfaceHolderAndroid&) = delete;

  // Attempting to access this if `weak_ptr_android_` is null is an
  // illegal operation and will crash. `weak_ptr_android_` should remain
  // valid so long as the tab is in a tab collection.
  tabs::TabInterface* GetTabInterface() override;

 private:
  base::WeakPtr<TabAndroid> weak_tab_android_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_INTERFACE_HOLDER_ANDROID_H_
