// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_UI_SUGGESTION_ACCESSIBILITY_LABEL_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_UI_SUGGESTION_ACCESSIBILITY_LABEL_H_

#include <memory>
#include "base/timer/timer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/controls/label.h"

namespace ui {
namespace ime {

// Label used for ChromeVox announcements via live regions.
class SuggestionAccessibilityLabel : public views::Label {
 public:
  SuggestionAccessibilityLabel();
  ~SuggestionAccessibilityLabel() override;

  // views::Label overrides
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Make announcement to ChromeVox with the given text
  void Announce(const std::u16string& text);

 private:
  // Callback used for delaying announcements
  void DoAnnouncement();

  // Used to delay the ChromeVox announcements. A delay is required as
  // announcements can "override" each other if they are triggered at
  // a similar time. Providing a delay prevents our announcement being
  // blocked by ChromeVox announcements triggered by text updates (i.e.
  // pressing a key will trigger an announcement of the letter found
  // on that key).
  std::unique_ptr<base::OneShotTimer> delay_timer_;
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_UI_SUGGESTION_ACCESSIBILITY_LABEL_H_
