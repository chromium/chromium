// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_INNER_TEXT_EXTRACTOR_H_
#define CHROME_BROWSER_COMPOSE_INNER_TEXT_EXTRACTOR_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "chrome/browser/compose/inner_text_extractor.h"
#include "chrome/browser/content_extraction/inner_text.h"

namespace content {
class WebContents;
}  // namespace content

// An helper class for observing the inner text of a webcontents.
class InnerTextExtractor {
 public:
  InnerTextExtractor();
  ~InnerTextExtractor();

  void Extract(content::WebContents* web_contents,
               base::OnceCallback<void(const std::string&)> callback);

 private:
  void InnerTextCallback(
      std::unique_ptr<content_extraction::InnerTextResult> result);

  std::vector<base::OnceCallback<void(const std::string&)>> callbacks_;

  raw_ptr<content::WebContents> previous_web_contents_;

  base::WeakPtrFactory<InnerTextExtractor> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_COMPOSE_INNER_TEXT_EXTRACTOR_H_
