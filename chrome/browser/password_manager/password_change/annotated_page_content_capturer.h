// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_ANNOTATED_PAGE_CONTENT_CAPTURER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_ANNOTATED_PAGE_CONTENT_CAPTURER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace password_manager {
class PasswordManagerClient;
}

namespace content {
class WebContents;
}

class AnnotatedPageContentCapturer {
 public:
  virtual ~AnnotatedPageContentCapturer() = default;

  using GetAIPageContentFunction =
      base::RepeatingCallback<void(blink::mojom::AIPageContentOptionsPtr,
                                   optimization_guide::OnAIPageContentDone)>;

  using FactoryCallback =
      base::RepeatingCallback<std::unique_ptr<AnnotatedPageContentCapturer>(
          content::WebContents*,
          blink::mojom::AIPageContentOptionsPtr,
          optimization_guide::OnAIPageContentDone)>;

  static std::unique_ptr<AnnotatedPageContentCapturer> Create(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      blink::mojom::AIPageContentOptionsPtr options,
      optimization_guide::OnAIPageContentDone callback);

#if defined(UNIT_TEST)
  static void SetFactoryForTesting(FactoryCallback factory) {
    GetFactory() = std::move(factory);
  }
#endif

 private:
  static FactoryCallback& GetFactory();
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_ANNOTATED_PAGE_CONTENT_CAPTURER_H_
