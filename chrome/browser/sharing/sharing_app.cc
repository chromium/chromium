// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_app.h"

SharingApp::SharingApp(const gfx::VectorIcon* vector_icon,
                       const gfx::Image& image,
                       base::string16 name,
                       std::string identifier)
    : vector_icon(vector_icon),
      image(image),
      name(std::move(name)),
      identifier(std::move(identifier)) {}

SharingApp::SharingApp(SharingApp&& other) = default;

SharingApp::~SharingApp() = default;
