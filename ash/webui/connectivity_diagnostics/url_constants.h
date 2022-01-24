// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CONNECTIVITY_DIAGNOSTICS_URL_CONSTANTS_H_
#define ASH_WEBUI_CONNECTIVITY_DIAGNOSTICS_URL_CONSTANTS_H_

namespace ash {

extern const char kChromeUIConnectivityDiagnosticsHost[];
extern const char kChromeUIConnectivityDiagnosticsUrl[];

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
using ::ash::kChromeUIConnectivityDiagnosticsUrl;
}

#endif  // ASH_WEBUI_CONNECTIVITY_DIAGNOSTICS_URL_CONSTANTS_H_
