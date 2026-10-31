// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/interactive_glic_test.h"

#include <ostream>

namespace glic::test {

std::ostream& operator<<(std::ostream& os, const TargetWebContents& value) {
  switch (value) {
    case TargetWebContents::kGlicWebUi:
      return os << "GlicWebUi";
    case TargetWebContents::kGlicClient:
      return os << "GlicClient";
  }
}

}  // namespace glic::test
