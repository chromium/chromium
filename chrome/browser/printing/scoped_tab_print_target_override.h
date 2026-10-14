// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_SCOPED_TAB_PRINT_TARGET_OVERRIDE_H_
#define CHROME_BROWSER_PRINTING_SCOPED_TAB_PRINT_TARGET_OVERRIDE_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace printing {

// Routes print commands for `tab` to `target`, a WebContents associated with
// `tab` that is not itself a tab (e.g. an overlay drawn over the tab's
// contents), for as long as this object is alive. At most one override can
// exist per tab at a time. Destroying this object cancels any open or pending
// print preview for `target`.
class [[nodiscard]] ScopedTabPrintTargetOverride {
 public:
  DECLARE_USER_DATA(ScopedTabPrintTargetOverride);

  ScopedTabPrintTargetOverride(tabs::TabInterface& tab,
                               content::WebContents& target);
  ScopedTabPrintTargetOverride(const ScopedTabPrintTargetOverride&) = delete;
  ScopedTabPrintTargetOverride& operator=(const ScopedTabPrintTargetOverride&) =
      delete;
  ~ScopedTabPrintTargetOverride();

  content::WebContents* target() const { return target_.get(); }

 private:
  base::WeakPtr<content::WebContents> target_;
  ui::ScopedUnownedUserData<ScopedTabPrintTargetOverride> scoped_user_data_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_SCOPED_TAB_PRINT_TARGET_OVERRIDE_H_
