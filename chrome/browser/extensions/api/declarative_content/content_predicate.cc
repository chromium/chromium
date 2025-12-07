// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_predicate.h"

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ContentPredicate::~ContentPredicate() = default;

bool ContentPredicate::IsIgnored() const {
  return false;
}

ContentPredicate::ContentPredicate() = default;

ContentPredicateFactory::~ContentPredicateFactory() = default;

ContentPredicateFactory::ContentPredicateFactory() = default;

}  // namespace extensions
