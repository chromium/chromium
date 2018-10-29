// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_ITEM_UMA_TYPE_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_ITEM_UMA_TYPE_H_

namespace ash {

// The different types of SystemTrayItems.
//
// NOTE: These values are used for UMA metrics so do NOT re-order this enum
// and only insert items before the COUNT item.
enum class SystemTrayItemUmaType {
  // SystemTrayItem's with this type are not recorded in the histogram.
  UMA_NOT_RECORDED = 0,
  // Used for testing purposes only.
  UMA_TEST = 1,
  UMA_ACCESSIBILITY = 2,
  UMA_AUDIO = 3,
  UMA_BLUETOOTH = 4,
  UMA_CAPS_LOCK = 5,
  UMA_CAST = 6,
  UMA_DATE = 7,
  UMA_DISPLAY = 8,
  UMA_DISPLAY_BRIGHTNESS = 9,
  UMA_ENTERPRISE = 10,
  UMA_IME = 11,
  UMA_MULTI_PROFILE_MEDIA = 12,
  UMA_NETWORK = 13,
  UMA_SETTINGS = 14,
  UMA_UPDATE = 15,
  UMA_POWER = 16,
  UMA_ROTATION_LOCK = 17,
  UMA_SCREEN_CAPTURE = 18,
  UMA_SCREEN_SHARE = 19,
  UMA_SESSION_LENGTH_LIMIT = 20,
  UMA_SMS = 21,
  UMA_SUPERVISED_USER = 22,
  UMA_TRACING = 23,
  UMA_USER = 24,
  UMA_VPN = 25,
  UMA_NIGHT_LIGHT = 26,
  UMA_QUIET_MODE = 27,
  UMA_COUNT = 28,
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_ITEM_UMA_TYPE_H_
