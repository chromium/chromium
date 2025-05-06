// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_APP_MENU_EXPORT_H_
#define ASH_APP_MENU_APP_MENU_EXPORT_H_

// Defines APP_MENU_EXPORT so that functionality implemented by the app_menu
// module can be exported to consumers.

#if defined(COMPONENT_BUILD)

#define APP_MENU_EXPORT __attribute__((visibility("default")))

#else  // defined(COMPONENT_BUILD)
#define APP_MENU_EXPORT
#endif

#endif  // ASH_APP_MENU_APP_MENU_EXPORT_H_
