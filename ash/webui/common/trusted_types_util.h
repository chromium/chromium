// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_TRUSTED_TYPES_UTIL_H_
#define ASH_WEBUI_COMMON_TRUSTED_TYPES_UTIL_H_

namespace content {
class WebUIDataSource;
}

namespace ash {

void EnableTrustedTypesCSP(content::WebUIDataSource* source);

}  // namespace ash

#endif  //  ASH_WEBUI_COMMON_TRUSTED_TYPES_UTIL_H_
