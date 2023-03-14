// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include "ash/style/style_viewer/system_ui_components_grid_view.h"
#include "ash/style/typography.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr size_t kGridViewRowNum = 21;
constexpr size_t kGridViewColNum = 1;
constexpr size_t kGridViewRowGroupSize = 1;
constexpr size_t kGridViewColGroupSize = 1;

struct TypographyEntry {
  std::u16string name;
  TypographyToken token;
  std::u16string sentence;
};

class TypographyGridView : public SystemUIComponentsGridView {
 public:
  TypographyGridView()
      : SystemUIComponentsGridView(kGridViewRowNum,
                                   kGridViewColNum,
                                   kGridViewRowGroupSize,
                                   kGridViewColGroupSize) {}
  TypographyGridView(const TypographyGridView&) = delete;
  TypographyGridView& operator=(const TypographyGridView&) = delete;

  ~TypographyGridView() override = default;

  void AddTypographySample(const TypographyEntry& entry) {
    views::Label* label =
        AddInstance(entry.name, std::make_unique<views::Label>(entry.sentence));
    TypographyProvider::Get()->StyleLabel(entry.token, *label);
  }
};

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreateTypographyInstancesGridView() {
  static const std::array<TypographyEntry, kGridViewRowNum> kEntries = {
      TypographyEntry{u"cros.typography.display0",
                      TypographyToken::kCrosDisplay0, u"This is a Headline"},
      {u"cros.typography.display1", TypographyToken::kCrosDisplay1,
       u"Slightly smaller headline"},
      {u"cros.typography.display2", TypographyToken::kCrosDisplay2,
       u"Yet Another Headline"},
      {u"cros.typography.display3", TypographyToken::kCrosDisplay3,
       u"Smaller Again"},
      {u"cros.typography.display3Regular",
       TypographyToken::kCrosDisplay3Regular, u"Display but normal weight"},
      {u"cros.typography.display4", TypographyToken::kCrosDisplay4,
       u"Smaller Again"},
      {u"cros.typography.display5", TypographyToken::kCrosDisplay5,
       u"Smaller Again"},
      {u"cros.typography.display6", TypographyToken::kCrosDisplay6,
       u"Smaller Again"},
      {u"cros.typography.display6Regular",
       TypographyToken::kCrosDisplay6Regular,
       u"Like display6 but normal weight"},
      {u"cros.typography.display7", TypographyToken::kCrosDisplay7,
       u"The Smallest Display Text"},

      {u"cros.typography.title1", TypographyToken::kCrosTitle1,
       u"The Largest Title"},
      {u"cros.typography.headline1", TypographyToken::kCrosHeadline1,
       u"A Healine in Text Format"},

      {u"cros.typography.button1", TypographyToken::kCrosButton1,
       u"Large button text"},
      {u"cros.typography.button2", TypographyToken::kCrosButton2,
       u"Less large button text"},

      {u"cros.typography.body0", TypographyToken::kCrosBody0,
       u"This is the largest body text."},
      {u"cros.typography.body1", TypographyToken::kCrosBody1,
       u"Slightly smaller body text."},
      {u"cros.typography.body2", TypographyToken::kCrosBody2,
       u"The smallest body text."},

      {u"cros.typography.annotation1", TypographyToken::kCrosAnnotation1,
       u"Large annotations"},
      {u"cros.typography.annotation2", TypographyToken::kCrosAnnotation2,
       u"Small annotations"},

      {u"cros.typography.label1", TypographyToken::kCrosLabel1,
       u"Large Label Text"},
      {u"cros.typography.label2", TypographyToken::kCrosLabel2,
       u"Smaller Label Text"}};

  auto grid_view = std::make_unique<TypographyGridView>();

  for (const auto& entry : kEntries) {
    grid_view->AddTypographySample(entry);
  }

  return grid_view;
}

}  // namespace ash
