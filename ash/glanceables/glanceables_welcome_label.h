// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_WELCOME_LABEL_H_
#define ASH_GLANCEABLES_GLANCEABLES_WELCOME_LABEL_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

namespace ash {

// Personalized greeting / welcome label that used on glanceables surfaces
// (welcome screen and overview mode).
class ASH_EXPORT GlanceablesWelcomeLabel : public views::Label {
 public:
  METADATA_HEADER(GlanceablesWelcomeLabel);

  GlanceablesWelcomeLabel();
  GlanceablesWelcomeLabel(const GlanceablesWelcomeLabel&) = delete;
  GlanceablesWelcomeLabel& operator=(const GlanceablesWelcomeLabel&) = delete;
  ~GlanceablesWelcomeLabel() override = default;

  // views::Label:
  void OnThemeChanged() override;

 private:
  std::u16string GetUserGivenName() const;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_WELCOME_LABEL_H_
