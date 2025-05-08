// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_

#include <string>

#include "base/supports_user_data.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace glic {

class GlicMediaContext : public base::SupportsUserData::Data {
 public:
  // Get or create for the current page for `web_contents`, or null if either
  // `web_contents` is null or it has no page.
  static GlicMediaContext* GetOrCreateFor(content::WebContents* web_contents);

  explicit GlicMediaContext(content::Page* page);
  ~GlicMediaContext() override;

  void OnResult(const media::SpeechRecognitionResult&);
  const std::string& GetContext() const;

 private:
  raw_ptr<content::Page> page_ = nullptr;
  std::string text_context_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_CONTEXT_H_
