// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_utils.h"

#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"

// TODO (jdh@): Move DIPSCookieMode and CookieAccessType into this file as well.

base::StringPiece GetHistogramPiece(DIPSRedirectType type) {
  // Any changes here need to be reflected in
  // tools/metrics/histograms/metadata/privacy/histograms.xml
  switch (type) {
    case DIPSRedirectType::kClient:
      return "Client";
    case DIPSRedirectType::kServer:
      return "Server";
  }
  DCHECK(false) << "Invalid DIPSRedirectType";
  return base::StringPiece();
}

const char* DIPSRedirectTypeToString(DIPSRedirectType type) {
  switch (type) {
    case DIPSRedirectType::kClient:
      return "Client";
    case DIPSRedirectType::kServer:
      return "Server";
  }
}

std::ostream& operator<<(std::ostream& os, DIPSRedirectType type) {
  return os << DIPSRedirectTypeToString(type);
}
