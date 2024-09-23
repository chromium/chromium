// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_KEY_ITEM_VIEW_H_
#define ASH_CAPTURE_MODE_KEY_ITEM_VIEW_H_

#include "ash/style/system_shadow.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// A view that displays a modifier or a key as a rounded corner UI component,
// which can contain a text label or an icon.
class KeyItemView : public views::View {
  METADATA_HEADER(KeyItemView, views::View)

 public:
  explicit KeyItemView(ui::KeyboardCode key_code);
  KeyItemView(const KeyItemView&) = delete;
  KeyItemView& operator=(const KeyItemView&) = delete;
  ~KeyItemView() override;

  // views::View:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  void SetIcon(const gfx::VectorIcon& icon);
  void SetText(const std::u16string& text);

  ui::KeyboardCode key_code() const { return key_code_; }
  views::ImageView* icon() const { return icon_; }

 private:
  const ui::KeyboardCode key_code_;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;

  // The shadow around each key item UI component in the combo. The shadow
  // should be `SystemShadowOnTextureLayer` as the corners are perfectly
  // rounded.
  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_KEY_ITEM_VIEW_H_
