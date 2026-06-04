// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/bcp47_extensions.h"

#include <string_view>

#include "base/check_op.h"
#include "base/i18n/language_tag.h"
#include "base/types/pass_key.h"

namespace base::i18n_extensions {

Extension::Extension(base::PassKey<base::LanguageTag>,
                     std::string_view extension)
    : extension_(extension) {
  CHECK_GE(extension_.size(), 4u);
  CHECK_EQ(extension_[1], '-');
}

UnicodeExtension::UnicodeExtension(base::PassKey<LanguageTag> pass_key,
                                   std::string_view extension)
    : Extension(pass_key, extension) {
  CHECK_EQ(singleton(), 'u');
}

PrivateUseSubtags::PrivateUseSubtags(base::PassKey<LanguageTag>,
                                     std::string_view private_use)
    : subtags_(std::string(private_use.substr(2))) {
  CHECK_GE(private_use.size(), 3u);
  CHECK_EQ(private_use[0], 'x');
  CHECK_EQ(private_use[1], '-');
}

}  // namespace base::i18n_extensions
