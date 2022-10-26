// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/pixel/ash_pixel_test_init_params.h"

namespace ash::pixel_test {

InitParams::InitParams(const std::string& param_screenshot_prefix,
                       const std::string& param_corpus)
    : screenshot_prefix(param_screenshot_prefix), corpus(param_corpus) {}

InitParams::InitParams(InitParams&&) = default;

InitParams& InitParams::operator=(InitParams&&) = default;

InitParams::~InitParams() = default;

}  // namespace ash::pixel_test
