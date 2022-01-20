// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ARC_TEXT_SELECTION_ACTION_LACROS_H_
#define CHROME_BROWSER_LACROS_ARC_TEXT_SELECTION_ACTION_LACROS_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "components/arc/common/intent_helper/text_selection_action_delegate.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace arc {

class TextSelectionActionLacros : public arc::TextSelectionActionDelegate {
 public:
  TextSelectionActionLacros();
  TextSelectionActionLacros(const TextSelectionActionLacros&) = delete;
  TextSelectionActionLacros& operator=(const TextSelectionActionLacros&) =
      delete;
  ~TextSelectionActionLacros() override;

  // arc::TextSelectionActionDelegate:
  bool RequestTextSelectionActions(
      const std::string& text,
      ui::ResourceScaleFactor scale_factor,
      RequestTextSelectionActionsCallback callback) override;

 private:
  // Convert vector of crosapi::mojom::TextSelectionActionPtr to vector of
  // arc::TextSelectionActionDelegate::TextSelectionAction.
  void OnRequestTextSelectionActions(
      RequestTextSelectionActionsCallback callback,
      crosapi::mojom::RequestTextSelectionActionsStatus status,
      std::vector<crosapi::mojom::TextSelectionActionPtr> actions);

  // This must come last to make sure weak pointers are invalidated first.
  base::WeakPtrFactory<TextSelectionActionLacros> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_LACROS_ARC_TEXT_SELECTION_ACTION_LACROS_H_
