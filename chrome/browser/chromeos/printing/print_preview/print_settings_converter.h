// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_SETTINGS_CONVERTER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_SETTINGS_CONVERTER_H_

#include "base/values.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"

namespace chromeos {

base::Value::Dict SerializePrintSettings(
    const crosapi::mojom::PrintSettingsPtr& settings);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_SETTINGS_CONVERTER_H_
