// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NAME_TAG_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NAME_TAG_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace arc::input_overlay {

// Create name tag with title and sub-title as:
// -----------
// |Title    |
// |Sub-title|
// -----------
class NameTag : public views::View {
 public:
  static std::unique_ptr<NameTag> CreateNameTag(
      const std::u16string& title,
      const std::u16string& sub_title);

  NameTag();
  NameTag(const NameTag&) = delete;
  NameTag& operator=(const NameTag&) = delete;
  ~NameTag() override;

  void SetTitle(const std::u16string& title);
  void SetSubtitle(const std::u16string& sub_title);

 private:
  void Init();

  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> subtitle_label_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_NAME_TAG_H_
