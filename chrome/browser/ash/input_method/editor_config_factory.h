// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONFIG_FACTORY_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONFIG_FACTORY_H_

#include "chrome/browser/ash/input_method/input_methods_by_language.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"

namespace ash::input_method {

orca::mojom::EditorConfigPtr BuildConfigFor(const LanguageCategory& language);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONFIG_FACTORY_H_
