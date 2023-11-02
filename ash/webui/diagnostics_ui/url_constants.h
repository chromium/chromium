// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_URL_CONSTANTS_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_URL_CONSTANTS_H_

namespace ash {

extern const char kChromeUIDiagnosticsAppHost[];
extern const char kChromeUIDiagnosticsAppUrl[];

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::kChromeUIDiagnosticsAppUrl;
}  // namespace chromeos

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_URL_CONSTANTS_H_
