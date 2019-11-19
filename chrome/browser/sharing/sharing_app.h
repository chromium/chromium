// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_APP_H_
#define CHROME_BROWSER_SHARING_SHARING_APP_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

// Represents an external app shown in sharing dialogs.
struct SharingApp {
 public:
  SharingApp(const gfx::VectorIcon* vector_icon,
             const gfx::Image& image,
             base::string16 name,
             std::string identifier);
  SharingApp(SharingApp&& other);
  ~SharingApp();

  const gfx::VectorIcon* vector_icon = nullptr;
  gfx::Image image;
  base::string16 name;
  std::string identifier;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharingApp);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_APP_H_
