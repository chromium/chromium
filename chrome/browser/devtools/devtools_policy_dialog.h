// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_POLICY_DIALOG_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_POLICY_DIALOG_H_

#include <map>
#include <memory>

#include "base/no_destructor.h"

namespace content {
class WebContents;
}

class DevToolsPolicyDialog {
 public:
  class TestObserver {
   public:
    virtual ~TestObserver() = default;
    virtual void OnDialogShown(DevToolsPolicyDialog* dialog) {}
    virtual void OnDialogClosed(DevToolsPolicyDialog* dialog) {}
    virtual void OnDialogDestroyed(DevToolsPolicyDialog* dialog) {}
  };

  static void Show(content::WebContents* web_contents);

  static void TestOnlyCloseDialog(content::WebContents* web_contents);

  ~DevToolsPolicyDialog();

  static void SetTestObserver(TestObserver* observer);

  static size_t GetCurrentDialogsSizeForTesting();

 private:
  explicit DevToolsPolicyDialog(content::WebContents* web_contents);

 private:
  static std::map<content::WebContents*, std::unique_ptr<DevToolsPolicyDialog>>&
  GetCurrentDialogs();
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_POLICY_DIALOG_H_
