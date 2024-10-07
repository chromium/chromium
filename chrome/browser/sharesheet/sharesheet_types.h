// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_TYPES_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_TYPES_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "chromeos/components/sharesheet/constants.h"
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

// The type of share action, when |TargetType| is kAction.
enum ShareActionType {
  kExample = 0,
  kNearbyShare = 1,
  kDriveShare = 2,
  kCopyToClipboardShare = 3,
};

struct TargetInfo {
  TargetInfo(TargetType type,
             std::optional<gfx::ImageSkia> icon,
             const std::u16string& launch_name,
             const std::u16string& display_name,
             const std::optional<ShareActionType>& share_action_type,
             const std::optional<std::u16string>& secondary_display_name,
             const std::optional<std::string>& activity_name,
             bool is_dlp_blocked);
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
  std::optional<gfx::ImageSkia> icon;

  // The string used to launch this target. Represents an Android package name
  // when the app type is kArc.
  std::u16string launch_name;

  // The string shown to the user to identify this target in the sharesheet
  // bubble.
  std::u16string display_name;

  // The enum that identifies the action type when |type| is kAction.
  std::optional<ShareActionType> share_action_type;

  // A secondary string below the |display_name| shown to the user to provide
  // additional information for this target. This will be populated by showing
  // the activity name in ARC apps.
  std::optional<std::u16string> secondary_display_name;

  // The activity of the app for the target. This only applies when the app type
  // is kArc.
  std::optional<std::string> activity_name;

  // Whether the target is blocked by Data Leak Prevention (DLP).
  bool is_dlp_blocked;
};

using DeliveredCallback = base::OnceCallback<void(SharesheetResult success)>;
using CloseCallback =
    base::OnceCallback<void(views::Widget::ClosedReason reason)>;
using ActionCleanupCallback = base::OnceCallback<void()>;

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_TYPES_H_
