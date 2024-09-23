// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SANITIZE_UI_SANITIZE_UI_DELEGATE_H_
#define ASH_WEBUI_SANITIZE_UI_SANITIZE_UI_DELEGATE_H_

namespace ash {

// A delegate which exposes the functionality to //chrome
class SanitizeUIDelegate {
 public:
  virtual ~SanitizeUIDelegate() = default;

  virtual void PerformSanitizeSettings() = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_SANITIZE_UI_SANITIZE_UI_DELEGATE_H_
