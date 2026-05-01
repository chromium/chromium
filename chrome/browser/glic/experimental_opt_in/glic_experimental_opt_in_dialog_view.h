// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_DIALOG_VIEW_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_DIALOG_VIEW_H_

#include "ui/base/interaction/element_identifier.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace glic {

class GlicExperimentalOptInDialogView : public views::DialogDelegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDialogElementId);

  explicit GlicExperimentalOptInDialogView(Profile* profile);
  GlicExperimentalOptInDialogView(const GlicExperimentalOptInDialogView&) =
      delete;
  GlicExperimentalOptInDialogView& operator=(
      const GlicExperimentalOptInDialogView&) = delete;
  ~GlicExperimentalOptInDialogView() override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_DIALOG_VIEW_H_
