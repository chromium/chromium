// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_TEST_FEATURE_H_
#define CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_TEST_FEATURE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_bridge.h"

namespace tabs {
class TabInterface;
}

namespace content {
class WebContents;
}

namespace context_sharing {

class TabBottomSheetTestFeature : public TabBottomSheetBridge::Observer {
 public:
  explicit TabBottomSheetTestFeature(tabs::TabInterface* tab);
  ~TabBottomSheetTestFeature() override;

  bool Show(bool animate, bool starts_expanded);
  void Close(bool animate);
  void SetWebContents(content::WebContents* web_contents);

  // TabBottomSheetBridge::Observer:
  void OnClosed() override;
  void OnSuppressed() override;
  void OnOpened(bool is_expanded) override;

  bool was_opened() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return was_opened_;
  }
  bool was_closed() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return was_closed_;
  }
  bool was_suppressed() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return was_suppressed_;
  }
  bool is_expanded() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_expanded_;
  }

  void set_on_opened_callback(base::RepeatingClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    on_opened_callback_ = std::move(callback);
  }
  void set_on_closed_callback(base::RepeatingClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    on_closed_callback_ = std::move(callback);
  }

  void ResetFlags() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    was_opened_ = false;
    was_closed_ = false;
    was_suppressed_ = false;
    is_expanded_ = false;
  }

 private:
  const raw_ref<tabs::TabInterface> tab_;
  std::unique_ptr<TabBottomSheetBridge> bridge_;

  bool was_opened_ = false;
  bool was_closed_ = false;
  bool was_suppressed_ = false;
  bool is_expanded_ = false;

  base::RepeatingClosure on_opened_callback_;
  base::RepeatingClosure on_closed_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace context_sharing

#endif  // CHROME_BROWSER_CONTEXT_SHARING_TAB_BOTTOM_SHEET_ANDROID_TAB_BOTTOM_SHEET_TEST_FEATURE_H_
