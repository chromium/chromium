// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_
#define ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_

#include <memory>
#include <optional>

#include "ash/public/cpp/style/color_provider.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageButton;
}  // namespace views

namespace ash {

class AssistantButton;
class AssistantButtonListener;
enum class AssistantButtonId;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantButton
    : public views::ImageButton {
  METADATA_HEADER(AssistantButton, views::ImageButton)

 public:
  // Initialization parameters for customizing the Assistant button.
  struct InitParams {
    InitParams();

    InitParams(InitParams&&);
    InitParams& operator=(InitParams&&) = default;

    ~InitParams();

    // Size of the Assistant button.
    int size_in_dip = 0;

    // Params for the icon.
    int icon_size_in_dip = 0;
    SkColor icon_color = gfx::kGoogleGrey700;
    // If both icon_color and icon_color_type are specified, icon_color_type
    // will be used.
    std::optional<ui::ColorId> icon_color_type;

    // ID of the localization string for the button's accessible name.
    std::optional<int> accessible_name_id;

    // ID of the localization string for the button's tooltip text.
    std::optional<int> tooltip_id;
  };

  AssistantButton(AssistantButtonListener* listener,
                  AssistantButtonId button_id);
  AssistantButton(const AssistantButton&) = delete;
  AssistantButton& operator=(const AssistantButton&) = delete;
  ~AssistantButton() override;

  // Creates a button with the default Assistant styles.
  static std::unique_ptr<AssistantButton> Create(
      AssistantButtonListener* listener,
      const gfx::VectorIcon& icon,
      AssistantButtonId button_id,
      InitParams params);

  AssistantButtonId GetAssistantButtonId() const { return id_; }

  // views::ImageButton:
  void OnBlur() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnFocus() override;

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  void OnButtonPressed();

  raw_ptr<AssistantButtonListener> listener_;
  const AssistantButtonId id_;

  // |icon_color_type_| and |icon_description_| are stored only when
  // icon_color_type is specified in InitParams.
  std::optional<ui::ColorId> icon_color_type_;
  std::optional<gfx::IconDescription> icon_description_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_BASE_ASSISTANT_BUTTON_H_
