// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_predicate.h"

namespace extensions {

ContentPredicate::~ContentPredicate() {}

bool ContentPredicate::IsIgnored() const {
  return false;
}

ContentPredicate::ContentPredicate() {}

ContentPredicateFactory::~ContentPredicateFactory() {}

ContentPredicateFactory::ContentPredicateFactory() {}

}  // namespace extensions
