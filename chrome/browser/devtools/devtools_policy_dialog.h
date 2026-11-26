// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_POLICY_DIALOG_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_POLICY_DIALOG_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/no_destructor.h"

enum class DevToolsBlockReason;

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

  // Shows the policy dialog for `web_contents` unless one is already open for
  // that tab. `web_contents` must not be null; it is not retained beyond the
  // lifetime of the dialog, which is closed when the tab is destroyed.
  // `reason` is the check that blocked DevTools (or view source) and is
  // recorded in DevTools.BlockedByPolicy.Reason. It must come from the
  // caller's own check rather than be derived from `web_contents`: the
  // blocked context can differ from the tab, e.g. a view-source load blocked
  // on the URL being viewed is shown on a tab that has not committed it.
  // Returns true if a new dialog was shown. Callers that only need the dialog
  // (e.g. view-source entry points) may ignore the result; it exists so
  // `DevToolsWindow` can avoid double-counting repeated attempts.
  static bool Show(content::WebContents* web_contents,
                   DevToolsBlockReason reason);

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
