// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DEFAULT_CONTENT_PREDICATE_EVALUATORS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DEFAULT_CONTENT_PREDICATE_EVALUATORS_H_

#include <memory>
#include <vector>

#include "chrome/browser/extensions/api/declarative_content/content_predicate_evaluator.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

std::vector<std::unique_ptr<ContentPredicateEvaluator>>
CreateDefaultContentPredicateEvaluators(
    content::BrowserContext* browser_context,
    ContentPredicateEvaluator::Delegate* delegate);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DEFAULT_CONTENT_PREDICATE_EVALUATORS_H_
