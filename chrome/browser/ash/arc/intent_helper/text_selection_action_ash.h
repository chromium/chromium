// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INTENT_HELPER_TEXT_SELECTION_ACTION_ASH_H_
#define CHROME_BROWSER_ASH_ARC_INTENT_HELPER_TEXT_SELECTION_ACTION_ASH_H_

#include <string>
#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/common/intent_helper/text_selection_action_delegate.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace arc {

// Ash-side TextSelectionActionDelegate handling.
class TextSelectionActionAsh : public TextSelectionActionDelegate {
 public:
  TextSelectionActionAsh();
  TextSelectionActionAsh(const TextSelectionActionAsh&) = delete;
  TextSelectionActionAsh& operator=(const TextSelectionActionAsh&) = delete;
  ~TextSelectionActionAsh() override;

  // TextSelectionActionDelegate:
  bool RequestTextSelectionActions(
      const std::string& text,
      ui::ResourceScaleFactor scale_factor,
      RequestTextSelectionActionsCallback callback) override;

 private:
  // Convertvector of mojom::TextSelectionActionPtr to vector of
  // RequestTextSelectionActionDelegate::TextSelectionAction.
  void OnRequestTextSelectionActions(
      RequestTextSelectionActionsCallback callback,
      std::vector<mojom::TextSelectionActionPtr> actions);

  // Convert arc::mojom::TextSelectionAction into TextSelectionAction.
  void ConvertTextSelectionAction(TextSelectionAction** converted_action,
                                  mojom::TextSelectionActionPtr action,
                                  base::OnceClosure callback,
                                  const gfx::ImageSkia& image);

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<TextSelectionActionAsh> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INTENT_HELPER_TEXT_SELECTION_ACTION_ASH_H_
