// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CLIPBOARD_INSERTION_H_
#define ASH_PICKER_PICKER_CLIPBOARD_INSERTION_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace ui {
class ClipboardData;
}

namespace ash {

ASH_EXPORT void InsertClipboardData(
    std::unique_ptr<ui::ClipboardData> data,
    base::OnceClosure do_web_paste,
    base::OnceCallback<void(bool)> done_callback);
}

#endif  // ASH_PICKER_PICKER_CLIPBOARD_INSERTION_H_
