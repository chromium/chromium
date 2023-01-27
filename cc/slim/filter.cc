// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/filter.h"

namespace cc::slim {

Filter::Filter(Type type, float amount) : type_(type), amount_(amount) {}

Filter::Filter(const Filter&) = default;

Filter& Filter::operator=(const Filter&) = default;

Filter::~Filter() = default;

bool Filter::operator==(const Filter& other) const {
  return type_ == other.type_ && amount_ == other.amount_;
}

}  // namespace cc::slim
