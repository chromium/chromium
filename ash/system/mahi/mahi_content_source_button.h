// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_CONTENT_SOURCE_BUTTON_H_
#define ASH_SYSTEM_MAHI_MAHI_CONTENT_SOURCE_BUTTON_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/view_factory.h"
#include "url/gurl.h"

namespace ash {

// A button which displays information about the current content source of the
// Mahi panel. Can be clicked to navigate to the content source page.
class ASH_EXPORT MahiContentSourceButton : public views::LabelButton {
  METADATA_HEADER(MahiContentSourceButton, views::LabelButton)

 public:
  MahiContentSourceButton();
  MahiContentSourceButton(const MahiContentSourceButton&) = delete;
  MahiContentSourceButton& operator=(const MahiContentSourceButton&) = delete;
  ~MahiContentSourceButton() override;

  // Updates the content source info to the source currently used by the Mahi
  // Manager instance.
  void RefreshContentSourceInfo();

 private:
  // Opens the content source page.
  void OpenContentSourcePage();

  // Url of the content source page.
  GURL content_source_url_;

  base::WeakPtrFactory<MahiContentSourceButton> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, MahiContentSourceButton, views::LabelButton)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::MahiContentSourceButton)

#endif  // ASH_SYSTEM_MAHI_MAHI_CONTENT_SOURCE_BUTTON_H_
