// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_action_properties.h"

#include <string>

#include "ui/base/mojom/window_open_disposition.mojom-shared.h"

namespace chrome {

DEFINE_UI_CLASS_PROPERTY_KEY(ui::mojom::WindowOpenDisposition,
                             kDispositionKey,
                             ui::mojom::WindowOpenDisposition::kUnknown)

DEFINE_UI_CLASS_PROPERTY_KEY(ActionInvocationSource,
                             kActionInvocationSourceKey,
                             ActionInvocationSource::kUnknown)

DEFINE_UI_CLASS_PROPERTY_KEY(int, kActionItemIconSizeDeltaKey, 0)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string,
                                   kActionSecondaryTextKeyInternal)

const ui::ClassProperty<std::u16string*>* const kActionSecondaryTextKey =
    kActionSecondaryTextKeyInternal;

}  // namespace chrome

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::mojom::WindowOpenDisposition)
DEFINE_UI_CLASS_PROPERTY_TYPE(chrome::ActionInvocationSource)
