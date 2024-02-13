// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_error_element_view.h"

#include "ash/assistant/model/ui/assistant_error_element.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

AssistantErrorElementView::AssistantErrorElementView(
    const AssistantErrorElement* error_element)
    : AssistantTextElementView(
          l10n_util::GetStringUTF8(error_element->message_id())) {}

BEGIN_METADATA(AssistantErrorElementView)
END_METADATA

}  // namespace ash
