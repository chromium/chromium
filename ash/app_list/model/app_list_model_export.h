// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_MODEL_EXPORT_H_
#define ASH_APP_LIST_MODEL_APP_LIST_MODEL_EXPORT_H_

// Defines APP_LIST_MODEL_EXPORT so that functionality implemented by the
// app_list_model module can be exported to consumers.

#if defined(COMPONENT_BUILD)

#if defined(APP_LIST_MODEL_IMPLEMENTATION)
#define APP_LIST_MODEL_EXPORT __attribute__((visibility("default")))
#else
#define APP_LIST_MODEL_EXPORT
#endif

#else  // defined(COMPONENT_BUILD)
#define APP_LIST_MODEL_EXPORT
#endif

#endif  // ASH_APP_LIST_MODEL_APP_LIST_MODEL_EXPORT_H_
