// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HELP_CONTENT_PROVIDER_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HELP_CONTENT_PROVIDER_H_

namespace ash {
namespace feedback {

class HelpContentProvider {
 public:
  HelpContentProvider();
  HelpContentProvider(const HelpContentProvider&) = delete;
  HelpContentProvider& operator=(const HelpContentProvider&) = delete;
  ~HelpContentProvider();
};

}  // namespace feedback
}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HELP_CONTENT_PROVIDER_H_
