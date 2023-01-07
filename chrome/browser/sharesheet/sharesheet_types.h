// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_TYPES_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_TYPES_H_

#include <string>

#include "base/functional/callback.h"
#include "chromeos/components/sharesheet/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"

namespace sharesheet {

// In DIP (Density Independent Pixel).
constexpr int kIconSize = 40;

// The type of a target.
enum class TargetType {
  kUnknown = 0,
  kArcApp,
  kWebApp,
  kAction,
};

struct TargetInfo {
  TargetInfo(TargetType type,
             const absl::optional<gfx::ImageSkia> icon,
             const std::u16string& launch_name,
             const std::u16string& display_name,
             const absl::optional<std::u16string>& secondary_display_name,
             const absl::optional<std::string>& activity_name);
  ~TargetInfo();

  // Allow move.
  TargetInfo(TargetInfo&& other);
  TargetInfo& operator=(TargetInfo&& other);

  // Allow copy.
  TargetInfo(const TargetInfo&);
  TargetInfo& operator=(const TargetInfo&);

  // The type of target that this object represents.
  TargetType type;

  // The icon to be displayed for this target in the sharesheet bubble.
  // DIP size must be kIconSize. Only apps will have icons as share actions will
  // have vector_icons that get generated when the view is displayed.
  absl::optional<gfx::ImageSkia> icon;

  // The string used to launch this target. Represents an Android package name
  // when the app type is kArc.
  std::u16string launch_name;

  // The string shown to the user to identify this target in the sharesheet
  // bubble.
  std::u16string display_name;

  // A secondary string below the |display_name| shown to the user to provide
  // additional information for this target. This will be populated by showing
  // the activity name in ARC apps.
  absl::optional<std::u16string> secondary_display_name;

  // The activity of the app for the target. This only applies when the app type
  // is kArc.
  absl::optional<std::string> activity_name;
};

using DeliveredCallback = base::OnceCallback<void(SharesheetResult success)>;
using CloseCallback =
    base::OnceCallback<void(views::Widget::ClosedReason reason)>;
using ActionCleanupCallback = base::OnceCallback<void()>;

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_TYPES_H_
