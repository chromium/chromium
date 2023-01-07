// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/i18n_util.h"
#include "base/i18n/rtl.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

GURL CreateLocalizedGURL(const std::string& url) {
  static constexpr char kLocaleParamKey[] = "hl";
  return net::AppendOrReplaceQueryParameter(GURL(url), kLocaleParamKey,
                                            base::i18n::GetConfiguredLocale());
}

}  // namespace util
}  // namespace assistant
}  // namespace ash