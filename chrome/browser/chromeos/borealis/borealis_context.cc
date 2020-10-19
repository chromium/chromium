// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context.h"

namespace borealis {

BorealisContext::~BorealisContext() = default;
BorealisContext::BorealisContext() = default;
BorealisContext::BorealisContext(Profile* profile) : profile_(profile) {}

}  // namespace borealis
