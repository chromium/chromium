// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"

#include <memory>

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/chip_view.h"
#include "ash/assistant/util/resource_util.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "url/gurl.h"

namespace ash {

SuggestionChipView::SuggestionChipView(AssistantViewDelegate* delegate,
                                       const AssistantSuggestion& suggestion)
    : ChipView(Type::kDefault),
      delegate_(delegate),
      suggestion_id_(suggestion.id) {
  SetText(base::UTF8ToUTF16(suggestion.text));

  const GURL& url = suggestion.icon_url;
  if (assistant::util::IsResourceLinkType(
          url, assistant::util::ResourceLinkType::kIcon)) {
    // Handle local images.
    SetIcon(assistant::util::CreateVectorIcon(url, ChipView::kIconSizeDip));
  } else if (!suggestion.icon_url.is_empty()) {
    // In the case of a remote image, this prevents layout jank that would
    // otherwise occur if we updated the view visibility only after the image
    // downloaded.
    MakeIconVisible();

    // Handle remote images.
    delegate_->DownloadImage(url, base::BindOnce(&SuggestionChipView::SetIcon,
                                                 weak_factory_.GetWeakPtr()));
  }
}

SuggestionChipView::~SuggestionChipView() = default;

BEGIN_METADATA(SuggestionChipView)
END_METADATA

}  // namespace ash
