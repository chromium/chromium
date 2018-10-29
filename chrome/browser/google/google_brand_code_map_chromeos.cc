// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_brand_code_map_chromeos.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"

namespace google_brand {
namespace chromeos {

std::string GetRlzBrandCode(
    const std::string& static_brand_code,
    base::Optional<policy::MarketSegment> market_segment) {
  struct BrandCodeValueEntry {
    const char* unenrolled_brand_code;
    const char* education_enrolled_brand_code;
    const char* enterprise_enrolled_brand_code;
  };
  static const base::NoDestructor<
      base::flat_map<std::string, BrandCodeValueEntry>>
      kBrandCodeMap({{"NPEC", {"BMGD", "YETH", "XAWJ"}},
                     {"VHUH", {"JYDF", "SFJY", "JMBU"}},
                     {"FWVK", {"MUTD", "GWKK", "SQSC"}},
                     {"HOMH", {"BXHI", "WXYD", "VRZY"}},
                     {"BDIW", {"UDUG", "TRYQ", "PWFV"}},
                     {"FQPJ", {"ZTQG", "ZNEO", "LYMZ"}},
                     {"NOMD", {"GZLV", "UNZR", "FVOP"}},
                     {"MCDN", {"BAOV", "GLVV", "XHGO"}},
                     {"TKER", {"KOSM", "IUCL", "LIIM"}},
                     {"PGQF", {"USPJ", "SFKO", "KNBH"}},
                     {"GJZV", {"BUSA", "GIOS", "UYOM"}},
                     {"FSGY", {"PJQC", "RHZW", "POVI"}},
                     {"IHZG", {"MLLN", "EZTK", "GJEJ"}},
                     {"PXDO", {"ZXCF", "TQWC", "HOAL"}}});

  const auto it = kBrandCodeMap->find(static_brand_code);
  if (it == kBrandCodeMap->end())
    return static_brand_code;
  const auto& entry = it->second;
  // An empty value indicates the device is not enrolled.
  if (!market_segment.has_value())
    return entry.unenrolled_brand_code;

  switch (market_segment.value()) {
    case policy::MarketSegment::EDUCATION:
      return entry.education_enrolled_brand_code;
    case policy::MarketSegment::ENTERPRISE:
    case policy::MarketSegment::UNKNOWN:
      // If the device is enrolled but market segment is unknown, it's fine to
      // treat it as enterprise enrolled.
      return entry.enterprise_enrolled_brand_code;
  }
  NOTREACHED();
  return static_brand_code;
}

}  // namespace chromeos
}  // namespace google_brand